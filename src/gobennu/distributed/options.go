package distributed

type Option func(*options)

type options struct {
	endpoint string
}

func newOptions(opts ...Option) options {
	result := options{}

	for _, o := range opts {
		o(&result)
	}

	return result
}

func Endpoint(e string) Option {
	return func(o *options) {
		o.endpoint = e
	}
}
