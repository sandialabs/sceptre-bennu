package distributed

import (
	"context"
	"strings"
	"time"

	"bennu/distributed/swig"
	"bennu/utils/logger"

	zmq "github.com/pebbe/zmq4/draft"
	"github.com/pkg/errors"
)

type ZMQSubscriber struct {
	options  options
	socket   *zmq.Socket
	handlers map[string][]SubscriptionHandler
}

func NewZMQSubscriber(opts ...Option) Subscriber {
	return &ZMQSubscriber{
		options:  newOptions(opts...),
		handlers: make(map[string][]SubscriptionHandler),
	}
}

func (this *ZMQSubscriber) Init(opts ...Option) error {
	for _, o := range opts {
		o(&this.options)
	}

	var err error

	this.socket, err = zmq.NewSocket(zmq.DISH)
	if err != nil {
		return err
	}

	logger.Info("connecting", "subscribe", this.options.endpoint)

	if err := this.socket.Bind(this.options.endpoint); err != nil {
		return errors.Wrap(err, "connecting to subscriber endpoint")
	}

	e := swig.NewEndpoint()
	e.SetStr(this.options.endpoint)

	if err := this.socket.Join(e.Hash()); err != nil {
		return errors.Wrap(err, "joining DISH group")
	}

	return nil
}

func (this *ZMQSubscriber) SubscribeToPoint(point string, handler SubscriptionHandler) error {
	this.handlers[point] = append(this.handlers[point], handler)

	logger.V(2).Info("subscribed", "point", point)

	return nil
}

func (this *ZMQSubscriber) Start(ctx context.Context) error {
	go func() {
		// Using a ZMQ Poller so we can set a timeout, which enables us to
		// check if the provided context is done periodically.
		poller := zmq.NewPoller()
		poller.Add(this.socket, zmq.POLLIN)

		logger.Info("listening for publishes")

		for {
			select {
			case <-ctx.Done():
				return
			default:
				// Time out after 1 second so we don't block until the next
				// publish, which could cause us to not return relatively soon
				// after the provided context is cancelled.
				sockets, err := poller.Poll(1 * time.Second)
				if err != nil {
					logger.Error(err, "polling the ZMQ DISH socket")
					continue
				}

				// poller.Poll returns a slice of sockets that had events
				// matching zmq.POLLIN. In our case, we only have one socket,
				// but we go ahead and loop over it since the slice could be
				// empty (easier to read than checking for zero length).
				for _, socket := range sockets {
					// We do a switch statement here, even though we only have one
					// socket, because the switch statement will cast the socket
					// to the correct type (zmq.DISH) for us (when we fall into the
					// case block, s will already be cast to zmq.DISH).
					switch s := socket.Socket; s {
					case this.socket:
						msg, err := s.RecvMessage(0)
						if err != nil {
							logger.Error(err, "receiving RADIO publish")
							continue
						}

						logger.V(9).Info("received publish", "msg", msg[0])

						for _, point := range strings.Split(msg[0], ",") {
							logger.V(9).Info("received point", "point", point)

							if data, err := NewPointDataFromPoint(point); err == nil {
								for _, handler := range this.handlers[data.Name] {
									handler <- data
								}
							}
						}
					}
				}
			}
		}
	}()

	return nil
}
