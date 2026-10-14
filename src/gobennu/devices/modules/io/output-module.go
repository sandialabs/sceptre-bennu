package io

import (
	"errors"
	"fmt"

	"bennu/devices/modules/memory"
	"bennu/distributed"
	"bennu/utils/logger"

	"github.com/beevik/etree"
)

type OutputModule struct {
	endpoint string
	points   []string

	updater distributed.Updater
	handler distributed.SubscriptionHandler
	memory  *memory.Memory

	configured bool
}

func NewOutputModule(memory *memory.Memory) *OutputModule {
	return &OutputModule{
		handler: make(distributed.SubscriptionHandler),
		memory:  memory,
	}
}

func (this *OutputModule) Init(u distributed.Updater) error {
	if !this.configured {
		return errors.New("output module not configured yet")
	}

	err := u.Init(distributed.Endpoint(this.endpoint))
	if err != nil {
		return fmt.Errorf("initializing updater: %w", err)
	}

	this.updater = u

	return nil
}

func (this *OutputModule) HandleTreeData(e *etree.Element) error {
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

func (this OutputModule) Update() error {
	batch := this.updater.GetBatchUpdater()

	// Locking the data memory so anything updating tags can also lock the data
	// memory to coordinate batch updates.
	this.memory.Lock()

	applyBinary, err := this.memory.UpdateBinaryDevices(
		// This function will get called multiple times, once for each binary tag
		// that has been updated recently.
		func(point string, status bool) error {
			logger.V(1).Info("handling cycle time update", "point", point, "status", status)

			batch.UpdateBinaryField(point, status)
			return nil
		},
	)

	if err != nil {
		logger.Error(err, "updating binary devices")
	}

	applyAnalog, err := this.memory.UpdateAnalogDevices(
		// This function will get called multiple times, once for each analog tag
		// that has been updated recently.
		func(point string, value float64) error {
			logger.V(1).Info("handling cycle time update", "point", point, "value", value)

			batch.UpdateAnalogField(point, value)
			return nil
		},
	)

	if err != nil {
		logger.Error(err, "updating analog devices")
	}

	this.memory.Unlock()

	err = batch.Commit()
	if err != nil {
		return fmt.Errorf("committing batch update: %w", err)
	}

	// Only mark writes as applied once the provider accepted them.
	applyBinary()
	applyAnalog()

	return nil
}

func (this *OutputModule) handleBinaryInput(e *etree.Element) error {
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
			this.points = append(this.points, point)
			this.memory.AddBinaryDevice(point)
		}
	}

	this.memory.AddBinaryModule(id, point)
	return nil
}

func (this *OutputModule) handleAnalogInput(e *etree.Element) error {
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
			this.points = append(this.points, point)
			this.memory.AddAnalogDevice(point)
		}
	}

	this.memory.AddAnalogModule(id, point)
	return nil
}
