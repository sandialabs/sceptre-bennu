package modules

import (
	"bennu/devices/modules/logic"
	"bennu/devices/modules/memory"
	"testing"

	"bennu"

	"github.com/beevik/etree"
)

var xml = `
	<TEST>
			<field-device>
					<tags>
							<internal-tag>
									<name>current</name>
									<value>1.0</value>
							</internal-tag>
							<internal-tag>
									<name>voltage</name>
									<value>1.0</value>
							</internal-tag>
							<internal-tag>
									<name>base_kv</name>
									<value>13.3</value>
							</internal-tag>
							<internal-tag>
									<name>voltage_pu_setpoint</name>
									<value>1.01</value>
							</internal-tag>
							<internal-tag>
									<name>voltage_angle</name>
									<value>180</value>
							</internal-tag>
							<internal-tag>
									<name>mw</name>
									<value>75</value>
							</internal-tag>
							<internal-tag>
									<name>mvar</name>
									<value>5</value>
							</internal-tag>
							<internal-tag>
									<name>freq</name>
									<value>60</value>
							</internal-tag>
							<internal-tag>
									<name>A</name>
									<value>100</value>
							</internal-tag>
							<internal-tag>
									<name>WRtg</name>
									<value>75</value>
							</internal-tag>
							<internal-tag>
								<name>WMaxLim_Ena</name>
								<status>true</status>
							</internal-tag>
							<internal-tag>
								<name>WMaxLimPct</name>
								<value>50</value>
							</internal-tag>
							<internal-tag>
									<name>VARtg</name>
									<value>75</value>
							</internal-tag>
							<internal-tag>
								<name>VArPct_Ena</name>
								<status>false</status>
							</internal-tag>
							<internal-tag>
								<name>VArMaxPct</name>
								<value>100</value>
							</internal-tag>
							<internal-tag>
								<name>Hz</name>
								<value>0</value>
							</internal-tag>
					</tags>

					<logic>
						<![CDATA[
							A = current

							AphA = A / 3
							AphB = A / 3
							AphC = A / 3

							phase_kv = base_kv * (3 ** 0.5)

							PhVphA = (phase_kv * 1000) / 3
							PhVphB = (phase_kv * 1000) / 3
							PhVphC = (phase_kv * 1000) / 3

							sprintf('MW: %f', mw)

							mw = WMaxLim_Ena ? WRtg * (WMaxLimPct / 100.0) : mw

							sprintf('MW: %f', mw)

							q = VArPct_Ena ? VARtg * (VArMaxPct / 100.0) : mvar
							i = (WRtg * 1000) / (voltage_pu_setpoint * base_kv * (3 ** 0.5))
							voltage = q / (i * sin(voltage_angle) * base_kv)

							Hz = freq
						]]>
					</logic>
			</field-device>
	</TEST>
`

func TestLogicModule(t *testing.T) {
	var (
		memory = memory.NewMemory()
		// iModule = io.NewInputModule(memory)
		// oModule = io.NewOutputModule(memory)
		lModule = logic.NewLogicModule(memory)
	)

	bennu.TreeDataHandlers["tags"] = memory
	// bennu.TreeDataHandlers["input"] = iModule
	// bennu.TreeDataHandlers["output"] = oModule
	bennu.TreeDataHandlers["logic"] = lModule

	doc := etree.NewDocument()
	doc.ReadFromString(xml)
	root := doc.SelectElement("TEST")

	t.Log("processing XML config")

	for _, child := range root.FindElement("field-device").ChildElements() {
		t.Log(child.Tag)

		if h, ok := bennu.TreeDataHandlers[child.Tag]; ok {
			t.Logf("processing tag %s", child.Tag)

			if err := h.HandleTreeData(child); err != nil {
				t.Error(err)
			}
		}
	}

	t.Log("getting points")

	points, _ := memory.GetPoints()

	t.Log("printing points")

	for p, v := range points {
		t.Logf("tag: %s, value: %v", p, v)
	}

	t.Log("executing logic")

	if err := lModule.Execute(); err != nil {
		t.Log(err)
		t.FailNow()
	}

	t.Log("getting points")

	points, _ = memory.GetPoints()

	t.Log("printing points")

	for p, v := range points {
		t.Logf("tag: %s, value: %v", p, v)
	}

	memory.UpdateAnalogDevices(func(tag string, value float64) error {
		t.Logf("updating tag: %s, value: %f", tag, value)
		return nil
	})
}
