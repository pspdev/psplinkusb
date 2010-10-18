/* Simple tool to parse a binary dump of the video stream */
#include <stdio.h>
#include "remotejoy.h"

int main(int argc, char **argv)
{
	struct JoyScrHeader head;
	FILE *fp;
	int frames = 0;
	int start_frame = 0;
	int end_frame = 0;
	double fTotal = 0.0;
	double fSeconds = 0.0;

	if(argc < 2)
	{
		printf("Usage: parse_dump file.bin\n");
		return 1;
	}

	fp = fopen(argv[1], "rb");
	if(fp == NULL)
	{
		printf("Could not open file %s\n", argv[1]);
		return 1;
	}

	while(1)
	{
		if(fread(&head, sizeof(head), 1, fp) != 1)
		{
			break;
		}

		if(head.magic != JOY_MAGIC)
		{
			printf("Invalid magic %08X\n", head.magic);
			continue;
		}

		if(head.size != 0)
		{
			if((start_frame == 0) && (frames != 0))
			{
				start_frame = head.ref;
			}
			end_frame = head.ref;
			printf("%d - size %d, ref %d\n", frames, head.size, head.ref);

			frames++;
			fseek(fp, head.size, SEEK_CUR);
		}
	}

	fclose(fp);

	fTotal = (double)frames;
	fSeconds = ((double)(end_frame-start_frame)) / 60.0;

	printf("Start: %d, end %d\n", start_frame, end_frame);
	printf("Number of frames: %d, total vsyncs: %d , seconds %f, avg fps %f\n", frames, end_frame - start_frame, fSeconds, 
			fTotal/fSeconds);
			
}
