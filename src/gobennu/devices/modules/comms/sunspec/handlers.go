package sunspec

import (
	"encoding/binary"
	"fmt"

	"bennu/utils/logger"

	"github.com/activeshadow/mbserver"
)

func (this SunSpecRTU) readHandler() mbserver.FunctionHandler {
	return func(s *mbserver.Server, frame mbserver.Framer) ([]byte, *mbserver.Exception) {
		var (
			data  = frame.GetData()
			start = int(binary.BigEndian.Uint16(data[0:2]))
			count = int(binary.BigEndian.Uint16(data[2:4]))
			size  int
		)

		logger.V(1).Info("SunSpec read request", "start", start, "count", count)

		data = []byte{}

		for i := start; i < start+count; {
			tag, ok := this.registers[i]
			if !ok {
				logger.Error(fmt.Errorf("IllegalDataAddress"), fmt.Sprintf("register does not exist: %d", i))
				return []byte{}, &mbserver.IllegalDataAddress
			}

			var (
				reg   = NewRegister(this.types[tag])
				value interface{}
				err   error
			)

			switch this.types[tag] {
			case "string8", "string16":
				value, err = this.memory.GetStringValueForTag(tag)
			default: // Everything else will (most likely) be an analog.
				value, err = this.memory.GetAnalogValueForTag(tag)
				if err != nil {
					// If there wasn't an analog value for the tag, check to see if there's a
					// binary status for it. If so, set the value based on the status. If not,
					// the `err` variable will still be set so things will fail as expected.
					status, statusErr := this.memory.GetBinaryStatusForTag(tag)
					if statusErr == nil {
						// Set the original `err` back to `nil` so we don't report a slave device
						// failure below.
						err = nil

						if status {
							value = float64(1)
						} else {
							value = float64(0)
						}
					}
				}
			}

			if err != nil {
				logger.Error(err, fmt.Sprintf("no value in datastore for tag %s", tag))
				return []byte{}, &mbserver.SlaveDeviceFailure
			}

			size = size + (reg.Length() * 2)
			data = append(data, reg.Bytes(value, this.scalings[i])...)

			i = i + reg.Length()
		}

		return append([]byte{byte(size)}, data...), &mbserver.Success
	}
}

func (this SunSpecRTU) writeHandler() mbserver.FunctionHandler {
	return func(s *mbserver.Server, frame mbserver.Framer) ([]byte, *mbserver.Exception) {
		var (
			data  = frame.GetData()
			start = int(binary.BigEndian.Uint16(data[0:2]))
			count = int(binary.BigEndian.Uint16(data[2:4]))
		)

		logger.V(1).Info("SunSpec write request", "start", start, "count", count)

		// offset after start register and register count
		b := 5

		// Lock the data memory to ensure all registers in this write request get
		// sent to the provider(s) as a batch update.
		this.memory.Lock()
		defer this.memory.Unlock()

		for i := start; i < start+count; {
			tag, ok := this.registers[i]
			if !ok {
				logger.Error(fmt.Errorf("IllegalDataAddress"), fmt.Sprintf("register does not exist: %d", i))
				return []byte{}, &mbserver.IllegalDataAddress
			}

			reg := NewRegister(this.types[tag])

			e := b + (reg.Length() * 2)
			d := data[b:e]

			value, err := reg.Value(d, this.scalings[i])
			if err != nil {
				logger.Error(err, fmt.Sprintf("converting data to value for register %d", i))
				return []byte{}, &mbserver.SlaveDeviceFailure
			}

			// If updating the analog tag fails, then convert the analog value to a
			// binary status and try updating the binary tag.
			if err := this.memory.UpdateAnalogTag(tag, value); err != nil {
				status := value != 0

				if err := this.memory.UpdateBinaryTag(tag, status); err != nil {
					logger.Error(err, fmt.Sprintf("updating tag %s for register %d", tag, i))
					return []byte{}, &mbserver.SlaveDeviceFailure
				}
			}

			b = e
			i = i + reg.Length()
		}

		return data[0:4], &mbserver.Success
	}
}

func notImplementedHandler() mbserver.FunctionHandler {
	return func(s *mbserver.Server, frame mbserver.Framer) ([]byte, *mbserver.Exception) {
		logger.Error(fmt.Errorf("IllegalFunction"), "non-implemented function called")
		return []byte{}, &mbserver.IllegalFunction
	}
}
