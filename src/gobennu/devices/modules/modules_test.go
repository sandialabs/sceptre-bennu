package modules

import (
	"testing"

	"bennu/devices/modules/logic"
	"bennu/devices/modules/memory"

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

			err := h.HandleTreeData(child)
			if err != nil {
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

	err := lModule.Execute()
	if err != nil {
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

// Integer-literal results (expr returns int) must not panic the logic module.
func TestLogicIntResult(t *testing.T) {
	mem := memory.NewMemory()
	l := logic.NewLogicModule(mem)

	doc := etree.NewDocument()
	doc.ReadFromString(`<tags><internal-tag><name>Hz</name><value>60</value></internal-tag></tags>`)
	err := mem.HandleTreeData(doc.Root())
	if err != nil {
		t.Fatal(err)
	}

	doc = etree.NewDocument()
	doc.ReadFromString(`<logic>Hz = 0</logic>`)
	err = l.HandleTreeData(doc.Root())
	if err != nil {
		t.Fatal(err)
	}

	err = l.Execute()
	if err != nil {
		t.Fatal(err)
	}

	v, err := mem.GetAnalogValueForTag("Hz")
	if err != nil || v != 0 {
		t.Fatalf("Hz = %v, %v; want 0", v, err)
	}
}

// A failed commit must leave the write pending; only apply() marks it done.
func TestPendingWriteSurvivesFailedCommit(t *testing.T) {
	mem := memory.NewMemory()

	doc := etree.NewDocument()
	doc.ReadFromString(`<tags><external-tag><name>kv</name><io>io1</io><type>analog</type></external-tag></tags>`)
	err := mem.HandleTreeData(doc.Root())
	if err != nil {
		t.Fatal(err)
	}

	mem.AddAnalogDevice("dev")
	mem.AddAnalogModule("io1", "dev")

	err = mem.UpdateAnalogTag("kv", 5)
	if err != nil {
		t.Fatal(err)
	}

	yielded := 0
	yield := func(string, float64) error { yielded++; return nil }

	// Commit "fails": apply is never called.
	_, err = mem.UpdateAnalogDevices(yield)
	if err != nil {
		t.Fatal(err)
	}

	if v, _ := mem.GetAnalogDeviceValue("dev"); v != 0 {
		t.Fatalf("device value = %v before commit; want 0", v)
	}

	// Next cycle retries the same write, and apply commits it.
	apply, err := mem.UpdateAnalogDevices(yield)
	if err != nil || yielded != 2 {
		t.Fatalf("retry: err=%v yielded=%d; want nil, 2", err, yielded)
	}

	apply()

	if v, _ := mem.GetAnalogDeviceValue("dev"); v != 5 {
		t.Fatalf("device value = %v after apply; want 5", v)
	}

	_, err = mem.UpdateAnalogDevices(yield)
	if err != nil || yielded != 2 {
		t.Fatalf("after apply: err=%v yielded=%d; want nil, 2", err, yielded)
	}
}

// One failing rule must not stop later rules from running and being written back.
func TestLogicContinuesPastFailingRule(t *testing.T) {
	mem := memory.NewMemory()
	l := logic.NewLogicModule(mem)

	doc := etree.NewDocument()
	doc.ReadFromString(
		`<tags><internal-tag><name>a</name><value>1</value></internal-tag><internal-tag><name>b</name><value>1</value></internal-tag></tags>`,
	)
	err := mem.HandleTreeData(doc.Root())
	if err != nil {
		t.Fatal(err)
	}

	doc = etree.NewDocument()
	doc.ReadFromString("<logic>\na = nope.field\nb = 7\n</logic>")
	err = l.HandleTreeData(doc.Root())
	if err != nil {
		t.Fatal(err)
	}

	err = l.Execute()
	if err == nil {
		t.Fatal("expected error from failing rule")
	}

	if v, _ := mem.GetAnalogValueForTag("b"); v != 7 {
		t.Fatalf("b = %v; want 7", v)
	}
}
