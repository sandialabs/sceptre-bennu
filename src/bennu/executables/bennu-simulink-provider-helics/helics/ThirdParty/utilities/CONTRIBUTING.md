# Contributing to GMLC-TDC libraries

:+1: First off, thank you for taking the time to contribute! :+1:

The following is a set of guidelines for contributing to GMLC-TDC and associated projects. These are suggested guidelines. Use your best judgment.

## Table Of Contents

## Licensing

The Utilities library is distributed under the terms of the BSD-3 clause license. All new
contributions must be made under this [LICENSE](LICENSE) in accordance with the Github [terms of service](https://help.github.com/en/articles/github-terms-of-service#6-contributions-under-repository-license), which uses inbound=outbound policy. By submitting a pull request you are acknowledging that you have the right to license your code under these terms.

## [Code of Conduct](.github/CODE_OF_CONDUCT.md)

If you just have a question check out [![Gitter chat](https://badges.gitter.im/GMLC-TDC/HELICS.png)](https://gitter.im/GMLC-TDC/HELICS)

## How Can I Contribute

### Reporting Bugs

This section guides you through submitting a bug report for the containers library. Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

When you are creating a bug report, please include as many details as possible. Frequently, helpful information includes operating system, version, compiler used, API, interface, and some details about the function or operation being performed.

> **Note:** If you find a **Closed** issue that seems like it is the same thing that you're experiencing, open a new issue and include a link to the original issue in the body of your new one.

### Suggesting Enhancements

This section guides you through submitting a feature request, or enhancement for the containers library, including completely new features and minor improvements to existing functionality.

Please include as much detail as possible including the steps that you imagine you would take if the feature you're requesting existed, and the rational and reasoning of why this feature would be a good idea for a co-simulation framework.

A pull request including a bug fix or feature will always be welcome.

#### Before Submitting An Enhancement Suggestion

- check the issue list for any similar issues

### Your First Code Contribution

Help with documentation and test cases is always welcome. Take a look at the [code coverage reports](https://codecov.io/gh/GMLC-TDC/utilities) for some ideas on places that need more testing

### Submitting a pull request

Typically you would want to submit a pull request against the develop branch. That branch gets merged into master periodically but the develop branch is the active development branch where all the code is tested and merged before making a release. There is a [Pull request template](.github/PULL_REQUEST_TEMPLATE.md) that will guide you through the needed information. The pull requests are run through several automated checks in Travis and Azure and for the most part must pass these tests before merging. The Codacy check is evaluated but not required as the checks are sometimes a bit aggressive.

## Styleguides

Code formatting is controlled via clang-format and a CI check is run on the code to verify the formmatting.
