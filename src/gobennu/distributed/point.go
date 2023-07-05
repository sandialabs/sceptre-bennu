package distributed

import (
	"strconv"
	"strings"

	"github.com/pkg/errors"
)

type PointData struct {
	Name   string
	Status bool
	Value  float64
}

func NewPointDataFromPoint(point string) (PointData, error) {
	var (
		datum = strings.Split(point, ":")
		data  PointData
	)

	if len(datum) != 2 {
		return data, errors.New("invalid point string")
	}

	if status, err := strconv.ParseBool(datum[1]); err == nil {
		data.Name = datum[0]
		data.Status = status

		return data, nil
	}

	if value, err := strconv.ParseFloat(datum[1], 64); err == nil {
		data.Name = datum[0]
		data.Value = value

		return data, nil
	}

	return data, errors.New("invalid point value")
}
