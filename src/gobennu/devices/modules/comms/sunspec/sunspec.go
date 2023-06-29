package sunspec

import (
	"context"
	"fmt"
	"strconv"

	"bennu/devices/modules/memory"
	"bennu/utils/logger"
	"bennu/version"

	"github.com/activeshadow/mbserver"
	"github.com/beevik/etree"
)

type SunSpecRTU struct {
	memory *memory.Memory

	ip   string
	port int

	scalings  map[int]int
	registers map[int]string

	types map[string]string
}

func NewSunSpecRTU(memory *memory.Memory) *SunSpecRTU {
	return &SunSpecRTU{
		memory:    memory,
		scalings:  make(map[int]int),
		registers: make(map[int]string),
		types:     make(map[string]string),
	}
}

func (this *SunSpecRTU) Start(ctx context.Context) error {
	endpoint := fmt.Sprintf("%s:%d", this.ip, this.port)

	logger.Info("starting SunSpec RTU", "version", version.Number, "endpoint", endpoint)

	server := mbserver.NewServer()

	// SunSpec doesn't use any of these function codes.
	server.RegisterFunctionHandler(1, notImplementedHandler())
	server.RegisterFunctionHandler(2, notImplementedHandler())
	server.RegisterFunctionHandler(4, notImplementedHandler())
	server.RegisterFunctionHandler(5, notImplementedHandler())
	server.RegisterFunctionHandler(6, notImplementedHandler())
	server.RegisterFunctionHandler(15, notImplementedHandler())

	server.RegisterFunctionHandler(3, this.readHandler())
	server.RegisterFunctionHandler(16, this.writeHandler())

	go func() {
		<-ctx.Done()
		server.Close()
	}()

	return server.ListenTCP(endpoint)
}

func (this *SunSpecRTU) HandleTreeData(e *etree.Element) error {
	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "ip":
			this.ip = child.Text()
		case "port":
			var err error
			this.port, err = strconv.Atoi(child.Text())
			if err != nil {
				return err
			}
		case "event-logging":
			// TODO
		case "register":
			if err := this.handleRegister(child); err != nil {
				return err
			}
		}
	}

	return nil
}

func (this *SunSpecRTU) handleRegister(e *etree.Element) error {
	var (
		addr  int
		scale int
		tag   string
	)

	for _, child := range e.ChildElements() {
		switch child.Tag {
		case "address":
			var err error
			addr, err = strconv.Atoi(child.Text())
			if err != nil {
				return err
			}
		case "scaling-factor":
			var err error
			scale, err = strconv.Atoi(child.Text())
			if err != nil {
				return err
			}
		default:
			tag = child.Text()

			if ok := this.memory.HasTag(tag); !ok {
				return fmt.Errorf("tag %s does not exist", tag)
			}

			this.types[tag] = child.Tag
		}
	}

	if addr == 0 || tag == "" {
		return fmt.Errorf("both address and tag must be provided")
	}

	this.scalings[addr] = scale
	this.registers[addr] = tag

	return nil
}
