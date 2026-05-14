typedef int dummy0;

#ifdef DO_KDEVTESTS

#include "tests.h"

#include "logbuf.h"
#include "mem/dmalloc.h"
#include "mem/mem.h"

bool dev__test__disk(kernel_disk_dev_t* dev) {
	bool retcode = true;

	dmalloc_ret_t dma = dmalloc32(512);
	uint16_t* test_buffer = (uint16_t*)dma.virt;

	for (int i = 0; i < 256; i++) {
		test_buffer[i] = (uint16_t)(0xABCD + i);
	}

	// test write (1 sector)
	if (dev->write_sectors(dev, 100, 1, dma) != 0) {
		logbuf_write("[ TEST ] FAILED: Write operation\n");
		retcode = false;
		goto cleanup;
	}

	memset(test_buffer, 0, 512);

	// test read (1 sector)
	if (dev->read_sectors(dev, 100, 1, dma) != 0) {
		logbuf_write("[ TEST ] FAILED: Read operation\n");
		retcode = false;
		goto cleanup;
	}

	// verify data integrity
	if (test_buffer[0] != 0xABCD || test_buffer[10] != (uint16_t)(0xABCD + 10)) {
		logbuf_write("[ TEST ] FAILED: Invalid data read back after write\n");
		retcode = false;
		goto cleanup;
	}

	// test zeroing
	memset(test_buffer, 0, 512);
	dev->write_sectors(dev, 100, 1, dma);

	memset(test_buffer, 0xFF, 512);
	dev->read_sectors(dev, 100, 1, dma);

	if (test_buffer[0] == 0 && test_buffer[255] == 0) {
		logbuf_printf("[ TEST ] SUCCESS: %s test passed\n", dev->model);
	} else {
		logbuf_write("[ TEST ] FAILED: clear verification failed.\n");
		retcode = false;
	}

cleanup:
	dmfree(dma.virt);
	return retcode;
}

#endif
