Alicanto is a new feature made to be a more simple co-simulation tool than HELICS.

The code is similar to the bennu HELICS code but stripped down.
Alicanto runs as a Subscriber and Client object. It takes in a json file which defines which points Alicanto cares about.
 - Subscriptions tell Alicanto which publish point (udp) to subscrie to and which point to keep track of
 - Endpoints tell Alicanto where to corelate that subscribed point to a server-endpoint

Usage:
`pybennu-alicanto -c alicanto.json -d DEBUG`

Please update this README as Alicanto is used more 