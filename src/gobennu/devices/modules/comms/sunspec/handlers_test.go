package sunspec

import (
	"testing"

	"bennu/devices/modules/memory"

	"github.com/activeshadow/mbserver"
	"github.com/stretchr/testify/assert"

	"github.com/beevik/etree"
)

func newMemory() *memory.Memory {
	var xml = `
		<TEST>
				<tags>
						<internal-tag>
								<name>var_I0</name>
								<value>5</value>
						</internal-tag>
						<internal-tag>
								<name>var_I1</name>
								<value>10</value>
						</internal-tag>
				</tags>
		</TEST>
	`

	doc := etree.NewDocument()
	doc.ReadFromString(xml)
	root := doc.SelectElement("TEST")

	tags := root.ChildElements()[0]

	memory := memory.NewMemory()
	memory.HandleTreeData(tags)

	return memory
}

func TestSunSpecIllegalFunctionHandler(t *testing.T) {
	var frame mbserver.TCPFrame

	mbserver.SetDataWithRegisterAndNumber(&frame, 1, 1)

	illegal := notImplementedHandler()
	out, err := illegal(nil, &frame)

	assert := assert.New(t)
	assert.Equal([]byte{}, out)
	assert.Equal(&mbserver.IllegalFunction, err)
}

func TestReadOutOfBoundsRegister(t *testing.T) {
	var (
		memory = memory.NewMemory()
		rtu    = NewSunSpecRTU(memory)
		frame  mbserver.TCPFrame
	)

	rtu.registers[40001] = "foobar"

	// Start at register 1, with a count of 1.
	mbserver.SetDataWithRegisterAndNumber(&frame, 1, 1)

	read := rtu.readHandler()
	out, err := read(nil, &frame)

	assert := assert.New(t)
	assert.Equal([]byte{}, out)
	assert.Equal(&mbserver.IllegalDataAddress, err)
}

func TestWriteOutOfBoundsRegister(t *testing.T) {
	var (
		memory = memory.NewMemory()
		rtu    = NewSunSpecRTU(memory)
		frame  mbserver.TCPFrame
	)

	rtu.registers[40001] = "foobar"

	// Start at register 1, with a count of 1, with a value of 1.
	mbserver.SetDataWithRegisterAndNumberAndValues(&frame, 1, 1, []uint16{1})

	write := rtu.writeHandler()
	out, err := write(nil, &frame)

	assert := assert.New(t)
	assert.Equal([]byte{}, out)
	assert.Equal(&mbserver.IllegalDataAddress, err)
}

func TestModbusReadHandler(t *testing.T) {
	var (
		memory = newMemory()
		rtu    = NewSunSpecRTU(memory)
		frame  mbserver.TCPFrame
	)

	rtu.types["var_I0"] = "uint16"
	rtu.registers[40000] = "var_I0"

	rtu.types["var_I1"] = "uint16"
	rtu.registers[40001] = "var_I1"

	// Start at register 40000, with a count of 2.
	mbserver.SetDataWithRegisterAndNumber(&frame, 40000, 2)

	read := rtu.readHandler()
	out, err := read(nil, &frame)

	assert := assert.New(t)
	assert.Equal([]byte{4, 0, 5, 0, 10}, out) // length = 4 (2 x 2), values = [5, 10]
	assert.Equal(&mbserver.Success, err)
}

func TestModbusWriteHandler(t *testing.T) {
	var (
		memory = newMemory()
		rtu    = NewSunSpecRTU(memory)
		frame  mbserver.TCPFrame
	)

	rtu.types["var_I0"] = "uint16"
	rtu.registers[40000] = "var_I0"

	// Start at register 1, with a count of 1, with a value of 1.
	mbserver.SetDataWithRegisterAndNumberAndValues(&frame, 40000, 1, []uint16{1})

	write := rtu.writeHandler()
	out, err := write(nil, &frame)

	val, _ := memory.GetAnalogValueForTag("var_I0")

	assert := assert.New(t)
	assert.Equal([]byte{0x9c, 0x40, 0x00, 0x01}, out) // start register = 40000 (0x9c40), count = 1
	assert.Equal(&mbserver.Success, err)
	assert.Equal(float64(1), val)
}
