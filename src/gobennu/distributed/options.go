package distributed

type Option func(*options)

type options struct {
	endpoint string
}

func newOptions(opts ...Option) options {
	options := options{}

	for _, o := range opts {
		o(&options)
	}

	return options
}

func Endpoint(e string) Option {
	return func(o *options) {
		o.endpoint = e
	}
}
