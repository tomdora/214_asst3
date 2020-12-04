all: Asst3.c TestClient.c
	gcc Asst3.c -pthread -o KKJserver
	gcc TestClient.c -o client
	
run: Asst3.c TestClient.c
	gcc Asst3.c -pthread -o KKJserver
	gcc TestClient.c -o client
	./KKJserver 3456
	
remove:
	rm -rf KKJserver client
