package distributed

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"bennu/utils/logger"

	zmq "github.com/pebbe/zmq4/draft"
	"github.com/pkg/errors"
)

// How long to wait on the provider before giving up on a request.
var requestTimeout = 5 * time.Second

type ZMQUpdater struct {
	// Used for locking access to the socket, since ZMQ sockets are not
	// thread-safe (https://godoc.org/github.com/pebbe/zmq4#NewSocket).
	sync.Mutex

	options options
	socket  *zmq.Socket
}

func NewZMQUpdater(opts ...Option) Updater {
	return &ZMQUpdater{
		options: newOptions(opts...),
	}
}

func (this *ZMQUpdater) Init(opts ...Option) error {
	for _, o := range opts {
		o(&this.options)
	}

	return this.connect()
}

// connect (re)creates the REQ socket. A REQ socket that timed out is stuck
// waiting for its reply, so a fresh one is needed to retry (Lazy Pirate).
func (this *ZMQUpdater) connect() error {
	if this.socket != nil {
		_ = this.socket.SetLinger(0) // best effort; the socket is being discarded
		_ = this.socket.Close()
		this.socket = nil
	}

	socket, err := zmq.NewSocket(zmq.REQ)
	if err != nil {
		return err
	}

	for _, setErr := range []error{
		socket.SetLinger(0),
		socket.SetSndtimeo(requestTimeout),
		socket.SetRcvtimeo(requestTimeout),
	} {
		if setErr != nil {
			_ = socket.Close()
			return setErr
		}
	}

	logger.Info("connecting", "request", this.options.endpoint)

	err = socket.Connect(this.options.endpoint)
	if err != nil {
		_ = socket.Close()
		return err
	}

	this.socket = socket

	return nil
}

func (this *ZMQUpdater) UpdateBinaryField(point string, status bool) error {
	logger.V(2).Info("sending update", "type", "binary", "point", point, "status", status)

	return this.sendUpdateMessage(fmt.Sprintf("%s:%v", point, status))
}

func (this *ZMQUpdater) UpdateAnalogField(point string, value float64) error {
	logger.V(2).Info("sending update", "type", "analog", "point", point, "value", value)

	return this.sendUpdateMessage(fmt.Sprintf("%s:%v", point, value))
}

func (this *ZMQUpdater) GetBatchUpdater() BatchUpdater {
	return newZMQBatchUpdater(this)
}

func (this *ZMQUpdater) sendUpdateMessage(msg string) error {
	this.Lock()
	defer this.Unlock()

	_, err := this.socket.SendMessage(fmt.Sprintf("WRITE=%s\x00", msg))
	if err != nil {
		this.reconnect()
		return errors.WithMessage(err, "sending provider request")
	}

	resp, err := this.socket.RecvMessage(0)
	if err != nil {
		this.reconnect()
		return errors.WithMessage(err, "receiving provider response")
	}

	datum := strings.Split(resp[0], "=")

	if len(datum) != 2 {
		return errors.WithMessage(errors.New("unexpected provider response"), resp[0])
	}

	switch datum[0] {
	case "ACK", "ack":
		logger.V(2).Info("received ACK from provider", "message", datum[1])
	case "ERR", "err":
		return errors.New(datum[1])
	default:
		return errors.WithMessage(errors.New("malformed provider response"), resp[0])
	}

	return nil
}

func (this *ZMQUpdater) reconnect() {
	err := this.connect()
	if err != nil {
		logger.Error(err, "reconnecting to provider")
	}
}

type zmqBatchUpdater struct {
	*ZMQUpdater

	updates []string
}

func newZMQBatchUpdater(parent *ZMQUpdater) *zmqBatchUpdater {
	return &zmqBatchUpdater{ZMQUpdater: parent}
}

func (this *zmqBatchUpdater) UpdateBinaryField(point string, status bool) {
	logger.V(2).Info("queueing batch update", "type", "binary", "point", point, "status", status)

	this.updates = append(this.updates, fmt.Sprintf("%s:%v", point, status))
}

func (this *zmqBatchUpdater) UpdateAnalogField(point string, value float64) {
	logger.V(2).Info("queueing batch update", "type", "analog", "point", point, "value", value)

	this.updates = append(this.updates, fmt.Sprintf("%s:%v", point, value))
}

func (this *zmqBatchUpdater) Commit() error {
	if len(this.updates) == 0 {
		return nil
	}

	logger.V(2).Info("committing batch update")

	return this.sendUpdateMessage(strings.Join(this.updates, ","))
}
