/* Copyright (c) 2008 The Board of Trustees of The Leland Stanford Junior University */
/* Copyright (c) 2011, 2012 Open Networking Foundation */
/* Copyright (c) 2012, 2013 Big Switch Networks, Inc. */
/* See the file LICENSE.loci which should have been included in the source distribution */

#if !defined(_OF_WIRE_BUF_H_)
#define _OF_WIRE_BUF_H_

#include <string.h>
#include <loci/loci_base.h>
#include <loci/of_object.h>
#include <loci/of_match.h>
#include <loci/of_buffer.h>

/****************************************************************
 *
 * Wire buffer declaration, constructor, data alloc, delete
 *
 ****************************************************************/

/* Maximum length of an OpenFlow message. All wire buffers allocated for
 * new objects (that don't come from a message) are this length to avoid
 * needing to grow the buffers. */
#define OF_WIRE_BUFFER_MAX_LENGTH 65535

/**
 * Buffer management structure
 */
typedef struct of_wire_buffer_s {
    /** Pointer to a monolithic data buffer */
    uint8_t *buf;

    /** Length of buffer actually allocated */
    int alloc_bytes;
    /** Current extent actually used */
    int current_bytes;
    /** If not NULL, use this to dealloc buf */
    of_buffer_free_f free;
} of_wire_buffer_t;

/**
 * Decouples object from underlying wire buffer
 *
 * Called a 'slice' in some places.
 */
typedef struct of_wire_object_s {
    /** A pointer to the underlying buffer's management structure. */
    of_wire_buffer_t *wbuf;  
    /** The start offset for this object relative to the start of the
     * underlying buffer */
    int obj_offset;
    /* Boolean, whether the object owns the wire buffer. */
    char owned;
} of_wire_object_t;

#define WBUF_BUF(wbuf) (wbuf)->buf
#define WBUF_ALLOC_BYTES(wbuf) (wbuf)->alloc_bytes
#define WBUF_CURRENT_BYTES(wbuf) (wbuf)->current_bytes

/**
 * For read access, throw an error code if current buffer
 * is not big enough.
 * @param wbuf Pointer to an of_wire_buffer_t structure
 * @param offset The extent of the buffer required
 */
#define OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset)                      \
    LOCI_ASSERT(((wbuf) != NULL) && (WBUF_BUF(wbuf) != NULL) &&             \
           (offset > 0) && (WBUF_CURRENT_BYTES(wbuf) >= offset))

/*
 * Index a wire buffer
 * Index a wire object (from obj_offset)
 * Index a LOCI object
 */

/**
 * Return a pointer to a particular offset in a wire buffer's data
 * @param wbuf Pointer to an of_wire_buffer_t structure
 * @param offset The location to reference
 */
#define OF_WIRE_BUFFER_INDEX(wbuf, offset) (&((WBUF_BUF(wbuf))[offset]))

/**
 * Return a pointer to a particular offset in the underlying buffer
 * associated with a wire object
 * @param wobj Pointer to an of_wire_object_t structure
 * @param offset The location to reference relative to the start of the object
 */
#define OF_WIRE_OBJECT_INDEX(wobj, offset) \
    OF_WIRE_BUFFER_INDEX((wobj)->wbuf, (offset) + (wobj)->obj_offset)

/****************************************************************
 * Object specific macros; of_object_t includes a wire_object
 ****************************************************************/

/**
 * Return a pointer to a particular offset in the underlying buffer
 * associated with a wire object
 * @param obj Pointer to an of_object_t object
 * @param offset The location to reference relative to the start of the object
 */
#define OF_OBJECT_BUFFER_INDEX(obj, offset) \
    OF_WIRE_OBJECT_INDEX(&((obj)->wire_object), offset)

/**
 * Return the absolute offset as an integer from a object-relative offset
 * @param obj Pointer to an of_wire_object_t structure
 * @param offset The location to reference relative to the start of the object
 */
#define OF_OBJECT_ABSOLUTE_OFFSET(obj, offset) \
    ((obj)->wire_object.obj_offset + offset)


/**
 * Map a generic object to the underlying wire buffer object (not the octets)
 *
 * Treat as private
 */
#define OF_OBJECT_TO_WBUF(obj) ((obj)->wire_object.wbuf)



/**
 * Minimum allocation size for wire buffer object
 */
#define OF_WIRE_BUFFER_MIN_ALLOC_BYTES 128

/**
 * Allocate a wire buffer object and the underlying data buffer.
 * The wire buffer is initally empty (current_bytes == 0).
 * @param a_bytes The number of bytes to allocate.
 * @returns A wire buffer object if successful or NULL
 */
static inline of_wire_buffer_t *
of_wire_buffer_new(int a_bytes)
{
    of_wire_buffer_t *wbuf;

    wbuf = (of_wire_buffer_t *)MALLOC(sizeof(of_wire_buffer_t));
    if (wbuf == NULL) {
        return NULL;
    }
    MEMSET(wbuf, 0, sizeof(of_wire_buffer_t));

    if (a_bytes < OF_WIRE_BUFFER_MIN_ALLOC_BYTES) {
        a_bytes = OF_WIRE_BUFFER_MIN_ALLOC_BYTES;
    }

    if ((wbuf->buf = (uint8_t *)MALLOC(a_bytes)) == NULL) {
        FREE(wbuf);
        return NULL;
    }
    MEMSET(wbuf->buf, 0, a_bytes);
    wbuf->current_bytes = 0;
    wbuf->alloc_bytes = a_bytes;

    return (of_wire_buffer_t *)wbuf;
}

/**
 * Allocate a wire buffer object and bind it to an existing buffer.
 * @param buf       Existing buffer.
 * @param bytes     Size of buf.
 * @param buf_free  Function called to deallocate buf.
 * @returns A wire buffer object if successful or NULL
 */
static inline of_wire_buffer_t *
of_wire_buffer_new_bind(uint8_t *buf, int bytes, of_buffer_free_f buf_free)
{
    of_wire_buffer_t *wbuf;

    wbuf = (of_wire_buffer_t *)MALLOC(sizeof(of_wire_buffer_t));
    if (wbuf == NULL) {
        return NULL;
    }

    wbuf->buf = buf;
    wbuf->free = buf_free;
    wbuf->current_bytes = bytes;
    wbuf->alloc_bytes = bytes;

    return (of_wire_buffer_t *)wbuf;
}

static inline void
of_wire_buffer_free(of_wire_buffer_t *wbuf)
{
    if (wbuf == NULL) return;

    if (wbuf->buf != NULL) {
        if (wbuf->free != NULL) {
            wbuf->free(wbuf->buf);
        } else {
            FREE(wbuf->buf);
        }
    }

    FREE(wbuf);
}

static inline void
of_wire_buffer_steal(of_wire_buffer_t *wbuf, uint8_t **buffer)
{
    *buffer = wbuf->buf;
    /* Mark underlying data buffer as taken */
    wbuf->buf = NULL;
    of_wire_buffer_free(wbuf);
}

/**
 * Increase the currently used length of the wire buffer.
 * Will fail an assertion if the allocated length is not long enough.
 *
 * @param wbuf Pointer to the wire buffer structure
 * @param bytes Total number of bytes buffer should grow to
 */

static inline void
of_wire_buffer_grow(of_wire_buffer_t *wbuf, int bytes)
{
    LOCI_ASSERT(wbuf != NULL);
    LOCI_ASSERT(wbuf->alloc_bytes >= bytes);
    if (bytes > wbuf->current_bytes) {
        wbuf->current_bytes = bytes;
    }
}

/* TBD */


/**
 * Get a uint8_t scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u8_get(of_wire_buffer_t *wbuf, int offset, uint8_t *value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint8_t));
    buf_u8_get(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Set a uint8_t scalar in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u8_set(of_wire_buffer_t *wbuf, int offset, uint8_t value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint8_t));
    buf_u8_set(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Get a uint16_t scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u16_get(of_wire_buffer_t *wbuf, int offset, uint16_t *value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint16_t));
    buf_u16_get(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Set a uint16_t scalar in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u16_set(of_wire_buffer_t *wbuf, int offset, uint16_t value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint16_t));
    buf_u16_set(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Get a uint32_t scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u32_get(of_wire_buffer_t *wbuf, int offset, uint32_t *value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint32_t));
    buf_u32_get(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Set a uint32_t scalar in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u32_set(of_wire_buffer_t *wbuf, int offset, uint32_t value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint32_t));
    buf_u32_set(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}


/**
 * Get a uint32_t scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_ipv4_get(of_wire_buffer_t *wbuf, int offset, of_ipv4_t *value)
{
    of_wire_buffer_u32_get(wbuf, offset, value);
}

/**
 * Set a ipv4 (uint32_t) scalar in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_ipv4_set(of_wire_buffer_t *wbuf, int offset, of_ipv4_t value)
{
    of_wire_buffer_u32_set(wbuf, offset, value);
}


/**
 * Get a uint64_t scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u64_get(of_wire_buffer_t *wbuf, int offset, uint64_t *value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint64_t));
    buf_u64_get(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Set a uint64_t scalar in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 *
 * The underlying buffer accessor funtions handle endian and alignment.
 */

static inline void
of_wire_buffer_u64_set(of_wire_buffer_t *wbuf, int offset, uint64_t value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + (int) sizeof(uint64_t));
    buf_u64_set(OF_WIRE_BUFFER_INDEX(wbuf, offset), value);
}

/**
 * Get a generic OF match structure from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to the structure to update
 *
 * NOT IMPLEMENTED.
 *
 */

static inline void
of_wire_buffer_match_get(int version, of_wire_buffer_t *wbuf, int offset,
                      of_match_t *value)
{
    LOCI_ASSERT(0);
}

/**
 * Set a generic OF match structure in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to the structure to store
 *
 * NOT IMPLEMENTED.
 *
 */

static inline void
of_wire_buffer_match_set(int version, of_wire_buffer_t *wbuf, int offset,
                      of_match_t *value)
{
    LOCI_ASSERT(0);
}

/**
 * Get a port description object from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to the structure to fill out
 *
 * NOT IMPLEMENTED.
 *
 * @fixme Where should this go?
 */

static inline void
of_wire_buffer_of_port_desc_get(int version, of_wire_buffer_t *wbuf, int offset,
                             void *value)
{
    LOCI_ASSERT(0);
}

/**
 * Set a port description object in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to the structure to fill out
 *
 * NOT IMPLEMENTED.
 *
 * @fixme Where should this go?
 */

static inline void
of_wire_buffer_of_port_desc_set(int version, of_wire_buffer_t *wbuf, int offset,
                             void *value)
{
    LOCI_ASSERT(0);
}

/**
 * Get a port number scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * Port numbers are version specific.
 */

static inline void
of_wire_buffer_port_no_get(int version, of_wire_buffer_t *wbuf, int offset,
                        of_port_no_t *value)
{
    uint16_t v16;
    uint32_t v32;

    switch (version) {
    case OF_VERSION_1_0:
        of_wire_buffer_u16_get(wbuf, offset, &v16);
        *value = v16;
        break;
    case OF_VERSION_1_1:
    case OF_VERSION_1_2:
    case OF_VERSION_1_3:
        of_wire_buffer_u32_get(wbuf, offset, &v32);
        *value = v32;
        break;
    default:
        LOCI_ASSERT(0);
    }
}

/**
 * Set a port number scalar from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store in the buffer
 *
 * Port numbers are version specific.
 */

static inline void
of_wire_buffer_port_no_set(int version, of_wire_buffer_t *wbuf, int offset,
                      of_port_no_t value)
{

    switch (version) {
    case OF_VERSION_1_0:
        of_wire_buffer_u16_set(wbuf, offset, (uint16_t)value);
        break;
    case OF_VERSION_1_1:
    case OF_VERSION_1_2:
    case OF_VERSION_1_3:
        of_wire_buffer_u32_set(wbuf, offset, (uint32_t)value);
        break;
    default:
        LOCI_ASSERT(0);
    }
}

/**
 * Get a flow mod command value from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 */

static inline void
of_wire_buffer_fm_cmd_get(int version, of_wire_buffer_t *wbuf, int offset,
                        of_fm_cmd_t *value)
{
    uint16_t v16;
    uint8_t v8;

    switch (version) {
    case OF_VERSION_1_0:
        of_wire_buffer_u16_get(wbuf, offset, &v16);
        *value = v16;
        break;
    case OF_VERSION_1_1:
    case OF_VERSION_1_2:
    case OF_VERSION_1_3:
        of_wire_buffer_u8_get(wbuf, offset, &v8);
        *value = v8;
        break;
    default:
        LOCI_ASSERT(0);
    }
}

/**
 * Set a flow mod command value in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 */

static inline void
of_wire_buffer_fm_cmd_set(int version, of_wire_buffer_t *wbuf, int offset,
                      of_fm_cmd_t value)
{
    switch (version) {
    case OF_VERSION_1_0:
        of_wire_buffer_u16_set(wbuf, offset, (uint16_t)value);
        break;
    case OF_VERSION_1_1:
    case OF_VERSION_1_2:
    case OF_VERSION_1_3:
        of_wire_buffer_u8_set(wbuf, offset, (uint8_t)value);
        break;
    default:
        LOCI_ASSERT(0);
    }
}

/**
 * Get a wild card bitmap value from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to store the value
 */

static inline void
of_wire_buffer_wc_bmap_get(int version, of_wire_buffer_t *wbuf, int offset,
                        of_wc_bmap_t *value)
{
    uint32_t v32;
    uint64_t v64;

    switch (version) {
    case OF_VERSION_1_0:
    case OF_VERSION_1_1:
        of_wire_buffer_u32_get(wbuf, offset, &v32);
        *value = v32;
        break;
    case OF_VERSION_1_2:
    case OF_VERSION_1_3:
        of_wire_buffer_u64_get(wbuf, offset, &v64);
        *value = v64;
        break;
    default:
        LOCI_ASSERT(0);
    }
}

/**
 * Set a wild card bitmap value in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value The value to store
 */

static inline void
of_wire_buffer_wc_bmap_set(int version, of_wire_buffer_t *wbuf, int offset,
                      of_wc_bmap_t value)
{
    switch (version) {
    case OF_VERSION_1_0:
    case OF_VERSION_1_1:
        of_wire_buffer_u32_set(wbuf, offset, (uint32_t)value);
        break;
    case OF_VERSION_1_2:
    case OF_VERSION_1_3:
        of_wire_buffer_u64_set(wbuf, offset, (uint64_t)value);
        break;
    default:
        LOCI_ASSERT(0);
    }
}

/* match bitmap and wildcard bitmaps followed the same pattern */
#define of_wire_buffer_match_bmap_get of_wire_buffer_wc_bmap_get
#define of_wire_buffer_match_bmap_set of_wire_buffer_wc_bmap_set

/* Derived functions, mostly for fixed length name strings */
#define of_wire_buffer_char_get of_wire_buffer_u8_get
#define of_wire_buffer_char_set of_wire_buffer_u8_set


/**
 * Get an octet object from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to where to put value
 *
 * of_octets_t is treated specially as the high level functions pass around
 * pointers for "get" operators.
 *
 * Important: The length of data to copy is stored in the value->bytes
 * variable.
 */

static inline void
of_wire_buffer_octets_data_get(of_wire_buffer_t *wbuf, int offset,
                               of_octets_t *value)
{
    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + OF_OCTETS_BYTES_GET(value));
    buf_octets_get(OF_WIRE_BUFFER_INDEX(wbuf, offset),
                   OF_OCTETS_POINTER_GET(value),
                   OF_OCTETS_BYTES_GET(value));
}

/**
 * Set an octet object in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param value Pointer to the octet stream to store
 * @param cur_len Current length of data in the buffer
 *
 * of_octets_t is treated specially as the high level functions pass around
 * pointers for "get" operators.
 *
 * @fixme Need to take into account cur_len
 */

static inline void
of_wire_buffer_octets_data_set(of_wire_buffer_t *wbuf, int offset,
                               of_octets_t *value, int cur_len)
{
    // FIXME need to adjust length of octets member in buffer
    LOCI_ASSERT(cur_len == 0 || cur_len == value->bytes);

    OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, offset + OF_OCTETS_BYTES_GET(value));
    buf_octets_set(OF_WIRE_BUFFER_INDEX(wbuf, offset),
                   OF_OCTETS_POINTER_GET(value),
                   OF_OCTETS_BYTES_GET(value));
}

static inline void
_wbuf_octets_set(of_wire_buffer_t *wbuf, int offset, uint8_t *src, int bytes) {
    of_octets_t octets;
    OF_OCTETS_POINTER_SET(&octets, src);
    OF_OCTETS_BYTES_SET(&octets, bytes);
    of_wire_buffer_octets_data_set(wbuf, offset, &octets, bytes);
}

static inline void
_wbuf_octets_get(of_wire_buffer_t *wbuf, int offset, uint8_t *dst, int bytes) {
    of_octets_t octets;
    OF_OCTETS_POINTER_SET(&octets, dst);
    OF_OCTETS_BYTES_SET(&octets, bytes);
    of_wire_buffer_octets_data_get(wbuf, offset, &octets);
}


/**
 * Get a MAC address from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param mac Pointer to the mac address location
 *
 * Uses the octets function.
 */

#define of_wire_buffer_mac_get(buf, offset, mac) \
    _wbuf_octets_get(buf, offset, (uint8_t *)mac, 6)

/**
 * Set a MAC address in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param mac The variable holding the mac address to store
 *
 * Uses the octets function.
 */

#define of_wire_buffer_mac_set(buf, offset, mac) \
    _wbuf_octets_set(buf, offset, (uint8_t *)&mac, 6)


/**
 * Get a port name string from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param mac The mac address
 *
 * Uses the octets function.
 */
#define of_wire_buffer_port_name_get(buf, offset, portname) \
    _wbuf_octets_get(buf, offset, (uint8_t *)portname, \
                           OF_MAX_PORT_NAME_LEN)

/**
 * Set a port name address in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param portname Where to store the port name
 *
 * Uses the octets function.
 */

#define of_wire_buffer_port_name_set(buf, offset, portname) \
    _wbuf_octets_set(buf, offset, (uint8_t *)portname, \
                           OF_MAX_PORT_NAME_LEN)


/**
 * Get a table name string from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param portname The port name
 *
 * Uses the octets function.
 */

#define of_wire_buffer_tab_name_get(buf, offset, tabname) \
    _wbuf_octets_get(buf, offset, (uint8_t *)tabname, \
                           OF_MAX_TABLE_NAME_LEN)

/**
 * Set a table name address in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param mac Where to store the table name
 *
 * Uses the octets function.
 */

#define of_wire_buffer_tab_name_set(buf, offset, tabname) \
    _wbuf_octets_set(buf, offset, (uint8_t *)tabname, \
                           OF_MAX_TABLE_NAME_LEN)

/**
 * Get a description string from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param desc Where to store the description string
 *
 * Uses the octets function.
 */

#define of_wire_buffer_desc_str_get(buf, offset, desc) \
    _wbuf_octets_get(buf, offset, (uint8_t *)desc, OF_DESC_STR_LEN)

/**
 * Set a description string in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param desc The description string
 *
 * Uses the octets function.
 */

#define of_wire_buffer_desc_str_set(buf, offset, desc) \
    _wbuf_octets_set(buf, offset, (uint8_t *)desc, OF_DESC_STR_LEN)

/**
 * Get a serial number string from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param sernum Where to store the serial number string
 *
 * Uses the octets function.
 */

#define of_wire_buffer_ser_num_get(buf, offset, sernum) \
    _wbuf_octets_get(buf, offset, (uint8_t *)sernum, OF_SERIAL_NUM_LEN)

/**
 * Set a serial number string in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param desc The serial number string
 *
 * Uses the octets function.
 */

#define of_wire_buffer_ser_num_set(buf, offset, sernum) \
    _wbuf_octets_set(buf, offset, (uint8_t *)sernum, OF_SERIAL_NUM_LEN)

/**
 * Get a str64 string from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param s The string
 *
 * Uses the octets function.
 */

#define of_wire_buffer_str64_get(buf, offset, s) \
    _wbuf_octets_get(buf, offset, (uint8_t *)s, 64)

/**
 * Set a str64 string in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param s Where to store the str64
 *
 * Uses the octets function.
 */

#define of_wire_buffer_str64_set(buf, offset, s) \
    _wbuf_octets_set(buf, offset, (uint8_t *)s, 64)

/**
 * Get an ipv6 address from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param addr Pointer to where to store the ipv6 address
 *
 * Uses the octets function.
 */

#define of_wire_buffer_ipv6_get(buf, offset, addr) \
    _wbuf_octets_get(buf, offset, (uint8_t *)addr, sizeof(of_ipv6_t))

/**
 * Set an ipv6 address in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param addr The variable holding ipv6 address to store
 *
 * Uses the octets function.
 */

#define of_wire_buffer_ipv6_set(buf, offset, addr) \
    _wbuf_octets_set(buf, offset, (uint8_t *)&addr, sizeof(of_ipv6_t))

/**
 * Get an bitmap_128 address from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param addr Pointer to where to store the bitmap_128 address
 *
 * Uses the octets function.
 */

#define of_wire_buffer_bitmap_128_get(buf, offset, addr) \
    (of_wire_buffer_u64_get(buf, offset, &addr->hi), of_wire_buffer_u64_get(buf, offset+8, &addr->lo))

/**
 * Set an bitmap_128 address in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param addr The variable holding bitmap_128 address to store
 *
 * Uses the octets function.
 */

#define of_wire_buffer_bitmap_128_set(buf, offset, addr) \
    (of_wire_buffer_u64_set(buf, offset, addr.hi), of_wire_buffer_u64_set(buf, offset+8, addr.lo))

/**
 * Get a checksum_128 from a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param checksum Pointer to where to store the checksum_128
 */

#define of_wire_buffer_checksum_128_get(buf, offset, checksum) \
    (of_wire_buffer_u64_get(buf, offset, &checksum->hi), of_wire_buffer_u64_get(buf, offset+8, &checksum->lo))

/**
 * Set a checksum_128 in a wire buffer
 * @param wbuf The pointer to the wire buffer structure
 * @param offset Offset in the wire buffer
 * @param checksum The variable holding checksum_128 to store
 */

#define of_wire_buffer_checksum_128_set(buf, offset, checksum) \
    (of_wire_buffer_u64_set(buf, offset, checksum.hi), of_wire_buffer_u64_set(buf, offset+8, checksum.lo))

/* Relocate data from start offset to the end of the buffer to a new position */
static inline void
of_wire_buffer_move_end(of_wire_buffer_t *wbuf, int start_offset, int new_offset)
{
    int bytes;
    int new_length;

    if (new_offset > start_offset) {
        bytes =  new_offset - start_offset;
        new_length = wbuf->alloc_bytes + bytes;
        OF_WIRE_BUFFER_ACCESS_CHECK(wbuf, new_length);
    } else {
        bytes =  start_offset - new_offset;
        new_length = wbuf->alloc_bytes - bytes;
    }

    MEMMOVE(&wbuf->buf[new_offset], &wbuf->buf[start_offset], bytes);
    wbuf->alloc_bytes = new_length;
}

/* Given a wire buffer object and the offset of the start of an of_match struct,
 * return its total length in the buffer
 */
static inline int
of_match_bytes(of_wire_buffer_t *wbuf, int offset) {
    uint16_t len;
    of_wire_buffer_u16_get(wbuf, offset + 2, &len);
    return OF_MATCH_BYTES(len);
}

extern void
of_wire_buffer_replace_data(of_wire_buffer_t *wbuf, 
                            int offset, 
                            int old_len,
                            uint8_t *data,
                            int new_len);

#endif /* _OF_WIRE_BUF_H_ */
