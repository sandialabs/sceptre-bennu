package bennu

import (
	"github.com/beevik/etree"
)

var TreeDataHandlers = make(map[string]TreeDataHandler)

type TreeDataHandler interface {
	HandleTreeData(*etree.Element) error
}

func ParseConfigFile(config string) error {
	doc := etree.NewDocument()
	if err := doc.ReadFromFile(config); err != nil {
		return err
	}

	root := doc.SelectElement("SCEPTRE")

	for _, child := range root.ChildElements() {
		if h, ok := TreeDataHandlers[child.Tag]; ok {
			if err := h.HandleTreeData(child); err != nil {
				return err
			}
		}
	}

	return nil
}
