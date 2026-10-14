package io

import (
	"context"
	"errors"
	"fmt"

	"bennu/devices/modules/memory"
	"bennu/distributed"
	"bennu/utils/logger"

	"github.com/beevik/etree"
)

type InputModule struct {
	endpoint string

	subscriber distributed.Subscriber
	handler    distributed.SubscriptionHandler
	memory     *memory.Memory

	configured bool
}

func NewInputModule(memory *memory.Memory) *InputModule {
	return &InputModule{
		handler: make(distributed.SubscriptionHandler),
		memory:  memory,
	}
}

func (this *InputModule) HandleTreeData(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "endpoint":
			this.endpoint = child.Text()
		case "binary":
			err := this.handleBinaryInput(child)
			if err != nil {
				return err
			}
		case "analog":
			err := this.handleAnalogInput(child)
			if err != nil {
				return err
			}
		}
	}

	this.configured = true
	return nil
}

func (this *InputModule) Init(s distributed.Subscriber) error {
	if !this.configured {
		return errors.New("input module not configured yet")
	}

	err := s.Init(distributed.Endpoint(this.endpoint))
	if err != nil {
		return fmt.Errorf("initializing subscriber: %w", err)
	}

	// Subscribe to device points for both this input module (r/o)
	// and the output module (r/w).
	for _, device := range this.memory.GetDevicePoints() {
		err = s.SubscribeToPoint(device, this.handler)
		if err != nil {
			return fmt.Errorf("subscribing to %s: %w", device, err)
		}
	}

	this.subscriber = s

	return nil
}

func (this InputModule) Start(ctx context.Context) error {
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case point := <-this.handler:
				logger.V(2).Info("handling publication", "point-data", fmt.Sprintf("%+v", point))

				var err error

				switch {
				case this.memory.IsBinaryDevice(point.Name):
					err = this.memory.SetBinaryDeviceStatus(point.Name, point.Status)
				case this.memory.IsAnalogDevice(point.Name):
					err = this.memory.SetAnalogDeviceValue(point.Name, point.Value)
				default:
					logger.V(9).Info("point not managed", "point", point.Name)
				}

				if err != nil {
					logger.Error(err, "handling publication", "point", point.Name)
				}
			}
		}
	}()

	return this.subscriber.Start(ctx)
}

func (this *InputModule) handleBinaryInput(e *etree.Element) error {
	var (
		id    string
		point string
	)

	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "id":
			id = child.Text()
		case "name":
			point = child.Text()
			this.memory.AddBinaryDevice(point)
		}
	}

	this.memory.AddBinaryModule(id, point)
	return nil
}

func (this *InputModule) handleAnalogInput(e *etree.Element) error {
	var (
		id    string
		point string
	)

	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "id":
			id = child.Text()
		case "name":
			point = child.Text()
			this.memory.AddAnalogDevice(point)
		}
	}

	this.memory.AddAnalogModule(id, point)
	return nil
}
