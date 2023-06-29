package io

import (
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
		return fmt.Errorf("output module not configured yet")
	}

	if err := u.Init(distributed.Endpoint(this.endpoint)); err != nil {
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
			if err := this.handleBinaryInput(child); err != nil {
				return err
			}
		case "analog":
			if err := this.handleAnalogInput(child); err != nil {
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

	err := this.memory.UpdateBinaryDevices(
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

	err = this.memory.UpdateAnalogDevices(
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

	if err := batch.Commit(); err != nil {
		return fmt.Errorf("committing batch update: %w", err)
	}

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
