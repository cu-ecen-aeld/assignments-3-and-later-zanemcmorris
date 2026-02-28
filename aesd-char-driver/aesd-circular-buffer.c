/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#include <stdio.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */

    if(buffer == NULL || entry_offset_byte_rtn == NULL){
        printf("Null addresses passed into find_entry_offet_for_fpos. Returning\n");
        return NULL;
    }

    // First we need to find the entry who has this character offset.
    int numCharsInBuffer = 0;
    int prevNumCharsInBuffer = 0;
    int entryIndex = buffer->out_offs;
    int entriesProcessed = 0;
    while(entriesProcessed < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
        prevNumCharsInBuffer = numCharsInBuffer;
        numCharsInBuffer += buffer->entry[entryIndex].size;
        printf("After considering entry %d, total chars is %d\n", entryIndex, numCharsInBuffer);
        if(numCharsInBuffer > char_offset){
            // Buffer now contains the char offset, and entryIndex holds the entry that just got us there.
            printf("Char offset should be in entry indexed %d\n", entryIndex);
            break;
        }

        entriesProcessed += 1;
        entryIndex += 1;
        if(entryIndex == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
            entryIndex = 0;
        }
    }

    if(numCharsInBuffer - 1 < char_offset){
        // If this is hit then the char offset does not map to an entry. 
        printf("Char offset not found in buffer. Returning\n");
        return NULL;
    }

    // We have total chars and the index of the start of the string of interest
    *entry_offset_byte_rtn = char_offset - prevNumCharsInBuffer; 

    return &buffer->entry[entryIndex];
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    static int hasBufferBeenFilled = 0;

    if(buffer == NULL || add_entry == NULL){
        printf("Null addresses passed into add_entry. Returning\n");
        return;
    }


    // Insert buffer entry
    buffer->entry[buffer->in_offs] = *add_entry;
    printf("Wrote entry to index %d w/ str: %s\n", buffer->in_offs, add_entry->buffptr);

    // Check if we wrote to end of list
    if(hasBufferBeenFilled && buffer->out_offs == buffer->in_offs){
        // We just overwrote oldest data, increment both out and in then
        buffer->out_offs += 1;
        if(buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
            buffer->out_offs = 0;
        }
    }

    // Increment our index tracker and wrap if needed
    buffer->in_offs += 1;
    if(buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED){
        // Buffer is at max, reset to zero
        buffer->in_offs = 0;
        hasBufferBeenFilled = 1;
    }

    printf("After inserting, in_offs=%d | out_offs=%d\n", buffer->in_offs, buffer->out_offs);

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
