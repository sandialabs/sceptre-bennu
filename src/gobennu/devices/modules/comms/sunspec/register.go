package sunspec

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
)

type Register struct {
	regType string
}

func NewRegister(t string) Register {
	return Register{regType: t}
}

func (this Register) Length() int {
	switch this.regType {
	case "acc16":
		return 1
	case "acc32":
		return 2
	case "acc64":
		return 4
	case "bitfield16":
		return 1
	case "bitfield32":
		return 2
	case "enum16":
		return 1
	case "enum32":
		return 2
	case "float32":
		return 2
	case "int16":
		return 1
	case "int32":
		return 2
	case "string8":
		return 8
	case "string16":
		return 16
	case "sunssf":
		return 1
	case "uint16":
		return 1
	case "uint32":
		return 2
	}

	return 0
}

func (this Register) Bytes(v interface{}, s int) []byte {
	var value interface{}

	// A mismatched type used to panic; it now encodes as zero.
	f, _ := v.(float64)
	str, _ := v.(string)

	switch this.regType {
	case "acc16", "uint16":
		value = uint16(f * math.Pow(10, -float64(s)))
	case "acc32", "uint32":
		value = uint32(f * math.Pow(10, -float64(s)))
	case "acc64":
		value = uint64(f * math.Pow(10, -float64(s)))
	case "bitfield16", "enum16":
		value = uint16(f)
	case "bitfield32", "enum32":
		value = uint32(f)
	case "float32":
		value = float32(f)
	case "int16":
		value = int16(f * math.Pow(10, -float64(s)))
	case "int32":
		value = int32(f * math.Pow(10, -float64(s)))
	case "string8":
		var value [16]byte

		copy(value[:], str)

		return value[:]
	case "string16":
		var value [32]byte

		copy(value[:], str)

		return value[:]
	case "sunssf":
		value = int16(f)
	}

	buf := new(bytes.Buffer)

	err := binary.Write(buf, binary.BigEndian, value)
	if err != nil {
		return nil
	}

	return buf.Bytes()
}

func (this Register) Value(d []byte, s int) (float64, error) {
	buf := bytes.NewReader(d)

	switch this.regType {
	case "acc16", "uint16":
		var value uint16

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, float64(s)), nil
	case "acc32", "uint32":
		var value uint32

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, float64(s)), nil
	case "acc64":
		var value uint64

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, float64(s)), nil
	case "bitfield16", "enum16":
		var value uint16

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value), nil
	case "bitfield32", "enum32":
		var value uint32

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value), nil
	case "float32":
		var value float32

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value), nil
	case "int16":
		var value int16

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, float64(s)), nil
	case "int32":
		var value int32

		err := binary.Read(buf, binary.BigEndian, &value)
		if err != nil {
			return 0, err
		}

		return float64(value) * math.Pow(10, float64(s)), nil
	default:
		return 0, fmt.Errorf("unexpected data type %s", this.regType)
	}
}
