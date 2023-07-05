package logic

import (
	"fmt"
	"math"
	"strings"

	"bennu/devices/modules/memory"
	"bennu/utils/logger"

	"github.com/antonmedv/expr"
	"github.com/antonmedv/expr/vm"
	"github.com/beevik/etree"
)

type LogicModule struct {
	logic  map[string]*vm.Program
	order  []string
	memory *memory.Memory
}

func NewLogicModule(memory *memory.Memory) *LogicModule {
	return &LogicModule{
		logic:  make(map[string]*vm.Program),
		memory: memory,
	}
}

func (this *LogicModule) HandleTreeData(e *etree.Element) error {
	lines := strings.Split(e.Text(), "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)

		if line == "" {
			continue
		}

		sides := strings.SplitN(line, " = ", 2)

		switch len(sides) {
		case 1:
			right := strings.TrimSpace(sides[0])

			if strings.HasPrefix(right, "sprintf") {
				left := fmt.Sprintf("sprintf%d", len(this.order))

				p, err := expr.Compile(right)
				if err != nil {
					return fmt.Errorf("compiling logic '%s': %w", right, err)
				}

				this.logic[left] = p
				this.order = append(this.order, left)
			}
		case 2:
			left := strings.TrimSpace(sides[0])
			right := strings.TrimSpace(sides[1])

			p, err := expr.Compile(right)
			if err != nil {
				return fmt.Errorf("compiling logic '%s': %w", right, err)
			}

			this.logic[left] = p
			this.order = append(this.order, left)
		}
	}

	return nil
}

func (this LogicModule) Execute() error {
	if len(this.logic) == 0 {
		return nil
	}

	var (
		env     = map[string]interface{}{"sin": math.Sin, "sprintf": fmt.Sprintf}
		results = make(map[string]interface{})
	)

	statuses, err := this.memory.GetBinaryPoints()
	if err != nil {
		// TODO: decide how to handle error. Note that some statuses may still get
		// returned with a non-nil error.
	}

	for tag, status := range statuses {
		env[tag] = status
	}

	values, err := this.memory.GetAnalogPoints()
	if err != nil {
		// TODO: decide how to handle error. Note that some values may still get
		// returned with a non-nil error.
	}

	for tag, value := range values {
		env[tag] = value
	}

	for _, v := range this.order {
		result, err := expr.Run(this.logic[v], env)
		if err != nil {
			return fmt.Errorf("running logic for tag '%s': %w", v, err)
		}

		if strings.HasPrefix(v, "sprintf") {
			fmt.Printf("LOGIC OUTPUT: %s\n", result)
			continue
		}

		// Add result back into env so following logic lines can have access to it.
		env[v] = result

		// Track calculated results to limit what's updated in the data store.
		results[v] = result
	}

	for tag, value := range results {
		if _, ok := statuses[tag]; ok {
			this.memory.UpdateBinaryTag(tag, value.(bool))
			continue
		}

		if _, ok := values[tag]; ok {
			this.memory.UpdateAnalogTag(tag, value.(float64))
			continue
		}

		logger.V(9).Info("logic result not in datastore (this is OK)", "tag", tag)
	}

	return nil
}
