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
		// Some statuses may still be returned with a non-nil error.
		logger.Error(err, "getting binary points")
	}

	for tag, status := range statuses {
		env[tag] = status
	}

	values, err := this.memory.GetAnalogPoints()
	if err != nil {
		// Some values may still be returned with a non-nil error.
		logger.Error(err, "getting analog points")
	}

	for tag, value := range values {
		env[tag] = value
	}

	var errs []string

	for _, v := range this.order {
		result, runErr := expr.Run(this.logic[v], env)
		if runErr != nil {
			// One bad rule must not disable the rest; keep going.
			logger.Error(runErr, "running logic", "tag", v)
			errs = append(errs, fmt.Sprintf("running logic for tag '%s': %v", v, runErr))
			continue
		}

		if strings.HasPrefix(v, "sprintf") {
			fmt.Printf("LOGIC OUTPUT: %s\n", result) //nolint:forbidigo // intentional operator-visible output
			continue
		}

		// Add result back into env so following logic lines can have access to it.
		env[v] = result

		// Track calculated results to limit what's updated in the data store.
		results[v] = result
	}

	for tag, value := range results {
		var updateErr error

		_, isStatus := statuses[tag]
		_, isValue := values[tag]

		switch {
		case isStatus:
			if b, isBool := value.(bool); isBool {
				updateErr = this.memory.UpdateBinaryTag(tag, b)
			} else {
				logger.Error(nil, "logic result is not a bool", "tag", tag, "value", value)
			}
		case isValue:
			switch n := value.(type) {
			case float64:
				updateErr = this.memory.UpdateAnalogTag(tag, n)
			case int:
				updateErr = this.memory.UpdateAnalogTag(tag, float64(n))
			case int64:
				updateErr = this.memory.UpdateAnalogTag(tag, float64(n))
			default:
				logger.Error(nil, "logic result is not a number", "tag", tag, "value", value)
			}
		default:
			logger.V(9).Info("logic result not in datastore (this is OK)", "tag", tag)
		}

		if updateErr != nil {
			logger.Error(updateErr, "updating tag", "tag", tag)
		}
	}

	if len(errs) > 0 {
		return fmt.Errorf("%s", strings.Join(errs, "; "))
	}

	return nil
}
