xm des client
xm des server
xm create -q server.conf
xm create -q client.conf
sleep 2
xenstore-write /local/domain/$(xm domid server)/client_dom $(xm domid client)
xenstore-write /local/domain/$(xm domid client)/server_dom $(xm domid server)
echo Server event channel port?
read port
xenstore-write /local/domain/$(xm domid client)/server_port $port
