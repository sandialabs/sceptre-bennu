package distributed

import "testing"

func TestNewPointDataFromPoint(t *testing.T) {
	cases := []struct {
		in      string
		want    PointData
		wantErr bool
	}{
		{"b:true", PointData{Name: "b", Status: true}, false},
		{"b:false", PointData{Name: "b"}, false},
		{"a:999.5", PointData{Name: "a", Value: 999.5}, false},
		{"a:-2", PointData{Name: "a", Value: -2}, false},
		{"nocolon", PointData{}, true},
		{"a:1:2", PointData{}, true},
		{"a:notanumber", PointData{}, true},
	}
	for _, c := range cases {
		got, err := NewPointDataFromPoint(c.in)
		if (err != nil) != c.wantErr {
			t.Errorf("%q: err = %v, wantErr %v", c.in, err, c.wantErr)
			continue
		}
		if !c.wantErr && got != c.want {
			t.Errorf("%q: got %+v, want %+v", c.in, got, c.want)
		}
	}
}

// batch payload joins queued updates with commas, same format the provider parses.
func TestBatchUpdaterQueue(t *testing.T) {
	b := newZMQBatchUpdater(&ZMQUpdater{})
	b.UpdateBinaryField("b1", true)
	b.UpdateAnalogField("a1", 1.5)
	want := []string{"b1:true", "a1:1.5"}
	if len(b.updates) != len(want) || b.updates[0] != want[0] || b.updates[1] != want[1] {
		t.Fatalf("updates = %v, want %v", b.updates, want)
	}
}
