/**************************************************************
 * Class::  CSC-415-01 Fall 2025
 * Name::  Arjun Bhagat
 * Student ID::  917129686
 * GitHub-Name::  smeerj
 * Project:: Assignment 5 – Buffered I/O read
 *
 * File:: b_io.c
 *
 * Description::
 *
 * This program implements a simple file system interface 
 * in C using similar open, read, and close functions to Unix systems.
 * 
 **************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20 // The maximum number of files open at one time

// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
{
	fileInfo *fi; // holds the low level system's file info
	char *buffer; // buffer for file data
	int bufferOffset;	//position in buffer
	int fileOffset;		// position in file
} b_fcb;

// static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init()
{
	if (startup)
		return; // already initialized

	// init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
	{
		fcbArray[i].fi = NULL; // indicates a free fcbArray
	}

	startup = 1;
}

// Method to get a free File Control Block FCB element
b_io_fd b_getFCB()
{
	for (int i = 0; i < MAXFCBS; i++)
	{
		if (fcbArray[i].fi == NULL)
		{
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;						 // Not thread safe but okay for this project
		}
	}

	return -1; // all in use
}


b_io_fd b_open(char *filename, int flags)
{
	if (startup == 0)
		b_init(); // Initialize our system

	b_io_fd fd = b_getFCB();
	if (fd < 0)
	{
		return -1; // No available FCB
	}

	// Get file information
	fileInfo *file_info = GetFileInfo(filename);
	if (!file_info)
	{
		return -1; // File not found
	}

	// Setup the FCB
	fcbArray[fd].fi = file_info; // Link to the file info
	fcbArray[fd].bufferOffset = 0;
	fcbArray[fd].fileOffset = 0;
	fcbArray[fd].buffer = malloc(B_CHUNK_SIZE); // Allocate buffer

	if (!fcbArray[fd].buffer)
	{
		return -1; // Memory allocation failed
	}

	return fd; // Return the file descriptor
}


int b_read(b_io_fd fd, char *destination, int count)
{
	if (startup == 0)
	{
		b_init(); // Initialize our system
	}

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
	{
		return (-1); // invalid file descriptor
	}

	// and check that the specified FCB is actually in use
	if (fcbArray[fd].fi == NULL) // File not open for this descriptor
	{
		return -1;
	}

	b_fcb *file = &fcbArray[fd];
	int bytesRead = 0;

	// If there isn't enough bytes left of the file, reset the amount we're
	// going to copy to just how many bytes are left
	if (file->fi->fileSize - file->fileOffset < count)
	{
		count = file->fi->fileSize - file->fileOffset;
	}

	if (!count) {
		return 0;
	}

	// If there is data in the buffer, copy it to the destination buffer
	if (file->bufferOffset > 0)
	{
		int remBufferSize = B_CHUNK_SIZE - file->bufferOffset;
		int bytesToRead = count;
		if (remBufferSize < count)
		{
			bytesToRead = remBufferSize;
		}

		memcpy(destination, file->buffer + file->bufferOffset, bytesToRead);

		file->bufferOffset = (bytesToRead + file->bufferOffset) % B_CHUNK_SIZE;
		file->fileOffset += bytesToRead;
		count -= bytesToRead;
		bytesRead += bytesToRead;
	}

	// If we can copy any complete blocks of data, copy them
	if (count % B_CHUNK_SIZE > 1)
	{
		int completeBlocks = file->fileOffset / B_CHUNK_SIZE;
		int blocksToRead = count / B_CHUNK_SIZE;

		LBAread(destination + bytesRead, blocksToRead, file->fi->location + completeBlocks);
		
		file->fileOffset += B_CHUNK_SIZE * blocksToRead;
		file->bufferOffset = 0;
		count -= B_CHUNK_SIZE * blocksToRead;
		bytesRead += B_CHUNK_SIZE * blocksToRead;
	}

	// If there are still some remaining bytes to be read, read them as well
	if (count > 0) 
	{
		int lastBlock = file->fileOffset / B_CHUNK_SIZE;
		LBAread(file->buffer, 1, file->fi->location + lastBlock);
		memcpy(destination + bytesRead, file->buffer, count);

		file->bufferOffset = count;
		file->fileOffset += count;
		bytesRead += count;
	}

	return bytesRead;
}


// b_close frees allocated memory and sets the FCB as unused
int b_close(b_io_fd fd)
{
	if (fd < 0 || fd >= MAXFCBS || fcbArray[fd].fi == NULL)
	{
		return -1; // Invalid fd or file not open
	}

	free(fcbArray[fd].buffer);	// Free the buffer
	fcbArray[fd].buffer = NULL;
	fcbArray[fd].fi = NULL;		// Mark the FCB as free

	return 0;
}
