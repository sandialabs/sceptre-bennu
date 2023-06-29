package comms

import (
	"context"
	"fmt"

	"bennu/devices/modules/comms/sunspec"
	"bennu/devices/modules/memory"

	"github.com/beevik/etree"
)

type Outstation interface {
	// should not block
	Start(context.Context) error
}

type CommsModule struct {
	memory      *memory.Memory
	outstations []Outstation
}

func NewCommsModule(memory *memory.Memory) *CommsModule {
	return &CommsModule{memory: memory}
}

func (this *CommsModule) HandleTreeData(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "sunspec-tcp-server":
			server := sunspec.NewSunSpecRTU(this.memory)

			if err := server.HandleTreeData(child); err != nil {
				return err
			}

			this.outstations = append(this.outstations, server)
		default:
			return fmt.Errorf("unknown comms tag %s", child.Tag)
		}
	}

	return nil
}

func (this CommsModule) Start(ctx context.Context) error {
	for _, outstation := range this.outstations {
		if err := outstation.Start(ctx); err != nil {
			return err
		}
	}

	return nil
}
