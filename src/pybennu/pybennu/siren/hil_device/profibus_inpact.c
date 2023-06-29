#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(int argc, const char **argv)
{
	int shmid;

	key_t key4 = 10004;
	unsigned int *slot4di32;
	key_t key5 = 10005;
	unsigned int *slot5di32;
	key_t key6 = 10006;
	unsigned short *slot6di16;
	key_t key7 = 10007;
	unsigned int *slot7do32;
	key_t key8 = 10008;
	unsigned int *slot8do32;
	key_t key9 = 10009;
	unsigned short *slot9ai8x16;
	key_t key10 = 10010;
	unsigned short *slot10ai8x16;
	key_t key11 = 10011;
	unsigned short *slot11ai8x16;

	if ((shmid = shmget(key4, sizeof(*slot4di32), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot4di32 = shmat(shmid, NULL, 0)) == (unsigned int *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key5, sizeof(*slot5di32), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot5di32 = shmat(shmid, NULL, 0)) == (unsigned int *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key6, sizeof(*slot6di16), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot6di16 = shmat(shmid, NULL, 0)) == (unsigned short *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key7, sizeof(*slot7do32), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot7do32= shmat(shmid, NULL, 0)) == (unsigned int *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key8, sizeof(*slot8do32), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot8do32 = shmat(shmid, NULL, 0)) == (unsigned int *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key9, 8*sizeof(*slot9ai8x16), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot9ai8x16 = shmat(shmid, NULL, 0)) == (unsigned short *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key10, 8*sizeof(*slot10ai8x16), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}

	if ((slot10ai8x16 = shmat(shmid, NULL, 0)) == (unsigned short *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}

	if ((shmid = shmget(key11, 8*sizeof(*slot11ai8x16), IPC_CREAT | 0666)) <0)
	{
		printf("Error getting shared memory id");
		exit(1);
	}
	if ((slot11ai8x16 = shmat(shmid, NULL, 0)) == (unsigned short *) -1)
	{
		printf("Error attaching shared memory id");
		exit(1);
	}


	slot9ai8x16[0] = 1;
	slot9ai8x16[1] = 8;
	*slot4di32 = 0xFFFFFFFE;
	while(1)
	{
		printf("Slot 4: %08x \n", *slot4di32);
		printf("Slot 5: %08x \n", *slot5di32);
		printf("Slot 6: %04x \n", *slot6di16);
		printf("Slot 7: %08x \n", *slot7do32);
		printf("Slot 8: %08x \n", *slot8do32);
		printf("Slot 9: %04x %04x %04x %04x %04x %04x %04x %04x \n",  slot9ai8x16[0], slot9ai8x16[1], slot9ai8x16[2], slot9ai8x16[3], slot9ai8x16[4], slot9ai8x16[5], slot9ai8x16[6], slot9ai8x16[7]);
		printf("Slot 10: %04x %04x %04x %04x %04x %04x %04x %04x \n",  slot10ai8x16[0], slot10ai8x16[1], slot10ai8x16[2], slot10ai8x16[3], slot10ai8x16[4], slot10ai8x16[5], slot10ai8x16[6], slot10ai8x16[7]);
		printf("Slot 11: %04x %04x %04x %04x %04x %04x %04x %04x \n",  slot11ai8x16[0], slot11ai8x16[1], slot11ai8x16[2], slot11ai8x16[3], slot11ai8x16[4], slot11ai8x16[5], slot11ai8x16[6], slot11ai8x16[7]);
		printf("\n");
		fflush(stdout);

		if (slot9ai8x16[0] > 10)
		{
			slot9ai8x16[0] = 1;
		}
		slot9ai8x16[0] = slot9ai8x16[0] + 1;
		sleep(1);
	}

	return 0;
}
