package main

import (
	"context"
	"os"

	"bennu"
	_ "bennu/devices/field-device"
	"bennu/utils/logger"
	"bennu/version"

	"github.com/urfave/cli"
)

func main() {
	var app = cli.NewApp()

	app.Name = "gobennu"
	app.Usage = "Golang-based SCEPTRE Applications"
	app.Authors = []cli.Author{{Name: "Sandia National Laboratories", Email: "emulytics@sandia.gov"}}
	app.Version = version.Number

	app.Flags = []cli.Flag{
		cli.StringFlag{
			Name:   "config-file",
			Usage:  "configuration file to load",
			Value:  "/etc/bennu/config.xml",
			EnvVar: "GOBENNU_CONFIG_FILE",
		},
		cli.IntFlag{
			Name:   "log-verbosity",
			Usage:  "set between 0 and 2 to increase verbosity (set to -1 to disable logging)",
			Value:  0,
			EnvVar: "GOBENNU_LOG_VERBOSITY",
		},
	}

	app.Action = func(c *cli.Context) error {
		verbosity := c.Int("log-verbosity")
		if verbosity < 0 {
			logger.Disable()
		} else {
			logger.Enable()
			logger.Verbosity(verbosity)
		}

		logger.V(9).Info("parsing config file", "file", c.String("config-file"))

		err := bennu.ParseConfigFile(c.String("config-file"))
		if err != nil && err != context.Canceled {
			logger.Error(err, "parsing config")
			return err
		}

		return nil
	}

	app.Run(os.Args)
}
