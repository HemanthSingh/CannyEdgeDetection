all:	
	cd dsp && make send;
	cd gpp && make send;

getit:
	rm -f ../pics/*out* 
	scp root@192.168.202:/home/root/esLAB/pool_notify/pics/* ../pics

viewit:	getit
	eog ../pics/*
