package distributed

import (
	"fmt"
	"strings"
	"sync"

	"bennu/utils/logger"

	zmq "github.com/pebbe/zmq4/draft"
	"github.com/pkg/errors"
)

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

	var err error

	this.socket, err = zmq.NewSocket(zmq.REQ)
	if err != nil {
		return err
	}

	logger.Info("connecting", "request", this.options.endpoint)

	if err := this.socket.Connect(this.options.endpoint); err != nil {
		return err
	}

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

	this.socket.SendMessage(fmt.Sprintf("WRITE=%s\x00", msg))

	resp, err := this.socket.RecvMessage(0)
	if err != nil {
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
