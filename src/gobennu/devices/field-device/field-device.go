package fielddevice

import (
	"context"
	"fmt"
	"time"

	"bennu"
	"bennu/devices/modules/comms"
	"bennu/devices/modules/io"
	"bennu/devices/modules/logic"
	"bennu/devices/modules/memory"
	"bennu/distributed"
	"bennu/utils/logger"
	"bennu/utils/sigterm"

	"github.com/beevik/etree"
)

type FieldDevice struct {
	memory  *memory.Memory
	inputs  *io.InputModule
	outputs *io.OutputModule
	logic   *logic.LogicModule
	comms   *comms.CommsModule

	name      string
	cycleTime time.Duration
}

func NewFieldDevice() *FieldDevice {
	memory := memory.NewMemory()

	return &FieldDevice{
		memory:  memory,
		inputs:  io.NewInputModule(memory),
		outputs: io.NewOutputModule(memory),
		logic:   logic.NewLogicModule(memory),
		comms:   comms.NewCommsModule(memory),
	}
}

func (this *FieldDevice) HandleTreeData(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "name":
			this.name = child.Text()
		case "cycle-time":
			duration, err := time.ParseDuration(child.Text() + "ms")
			if err != nil {
				return err
			}

			this.cycleTime = duration
		case "tags":
			err := this.memory.HandleTreeData(child)
			if err != nil {
				return err
			}
		case "input":
			err := this.inputs.HandleTreeData(child)
			if err != nil {
				return err
			}
		case "output":
			err := this.outputs.HandleTreeData(child)
			if err != nil {
				return err
			}
		case "logic":
			err := this.logic.HandleTreeData(child)
			if err != nil {
				return err
			}
		case "comms":
			err := this.comms.HandleTreeData(child)
			if err != nil {
				return err
			}
		default:
			return fmt.Errorf("unknown field device element %s", child.Tag)
		}
	}

	var (
		ctx = sigterm.CancelContext(context.Background())
		sub = distributed.NewZMQSubscriber()
		cli = distributed.NewZMQUpdater()
	)

	logger.V(9).Info("initializing input module")

	err := this.inputs.Init(sub)
	if err != nil {
		return err
	}

	logger.V(9).Info("starting input module")

	err = this.inputs.Start(ctx)
	if err != nil {
		return err
	}

	logger.V(9).Info("initializing output module")

	err = this.outputs.Init(cli)
	if err != nil {
		return err
	}

	ticker := time.NewTicker(this.cycleTime)

	go func() {
		for {
			select {
			case <-ctx.Done():
				ticker.Stop()
				return
			case now := <-ticker.C:
				logger.V(9).Info("cycle time ticker", "time", now)

				this.runCycle()
			}
		}
	}()

	logger.V(9).Info("starting comms module")

	err = this.comms.Start(ctx)
	if err != nil {
		return err
	}

	<-ctx.Done()
	return ctx.Err()
}

func init() {
	bennu.TreeDataHandlers["field-device"] = NewFieldDevice()
}

// runCycle runs one freeze/logic/output/unfreeze cycle. Unfreeze is deferred so
// a failure or panic in an earlier step can't leave memory frozen forever.
func (this *FieldDevice) runCycle() {
	logger.V(9).Info("freezing memory")

	err := this.memory.Freeze()
	if err != nil {
		logger.Error(err, "freezing memory")

		return
	}

	defer func() {
		logger.V(9).Info("unfreezing memory")

		unfreezeErr := this.memory.Unfreeze()
		if unfreezeErr != nil {
			logger.Error(unfreezeErr, "unfreezing memory")
		}
	}()

	logger.V(9).Info("executing logic")

	err = this.logic.Execute()
	if err != nil {
		logger.Error(err, "executing logic")
	}

	logger.V(9).Info("updating outputs")

	err = this.outputs.Update()
	if err != nil {
		logger.Error(err, "updating outputs")
	}
}
