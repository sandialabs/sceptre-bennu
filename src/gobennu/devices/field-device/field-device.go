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
			if err := this.memory.HandleTreeData(child); err != nil {
				return err
			}
		case "input":
			if err := this.inputs.HandleTreeData(child); err != nil {
				return err
			}
		case "output":
			if err := this.outputs.HandleTreeData(child); err != nil {
				return err
			}
		case "logic":
			if err := this.logic.HandleTreeData(child); err != nil {
				return err
			}
		case "comms":
			if err := this.comms.HandleTreeData(child); err != nil {
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

	if err := this.inputs.Init(sub); err != nil {
		return err
	}

	logger.V(9).Info("starting input module")

	if err := this.inputs.Start(ctx); err != nil {
		return err
	}

	logger.V(9).Info("initializing output module")

	if err := this.outputs.Init(cli); err != nil {
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

				logger.V(9).Info("freezing memory")

				this.memory.Freeze()

				logger.V(9).Info("executing logic")

				if err := this.logic.Execute(); err != nil {
					logger.Error(err, "executing logic")
				}

				logger.V(9).Info("updating outputs")

				if err := this.outputs.Update(); err != nil {
					logger.Error(err, "updating outputs")
				}

				logger.V(9).Info("unfreezing memory")

				this.memory.Unfreeze()
			}
		}
	}()

	logger.V(9).Info("starting comms module")

	if err := this.comms.Start(ctx); err != nil {
		return err
	}

	<-ctx.Done()
	return ctx.Err()
}

func init() {
	bennu.TreeDataHandlers["field-device"] = NewFieldDevice()
}
