typedef int dummy0;

#ifdef DO_KDEVTESTS

#include "tests.h"

#include "logbuf.h"
#include "mem/pma.h"
#include "mem/mem.h"

bool dev__test__disk(kernel_disk_dev_t* dev) {
	bool retcode = true;

	uintptr_t phys_addr = pma_alloc_pages(1);
	uint16_t* test_buffer = (uint16_t*)(phys_addr + hhdm_offset);

	for (int i = 0; i < 256; i++) {
		test_buffer[i] = (uint16_t)(0xABCD + i);
	}

	// test write (1 sector)
	if (dev->write_sectors(dev, 100, 1, test_buffer) != 0) {
		logbuf_write("[ TEST ] FAILED: Write operation\n");
		retcode = false;
		goto cleanup;
	}

	memset(test_buffer, 0, 512);

	// test read (1 sector)
	if (dev->read_sectors(dev, 100, 1, test_buffer) != 0) {
		logbuf_write("[ TEST ] FAILED: Read operation\n");
		retcode = false;
		goto cleanup;
	}

	// verify data integrity
	if (test_buffer[0] != 0xABCD || test_buffer[10] != 0xABCD + 10) {
		logbuf_write("[ TEST ] FAILED: Invalid data read back after write\n");
		retcode = false;
		goto cleanup;
	}

	// test zeroing
	memset(test_buffer, 0, 512);
	dev->write_sectors(dev, 100, 1, test_buffer);

	memset(test_buffer, 0xFF, 512);
	dev->read_sectors(dev, 100, 1, test_buffer);

	if (test_buffer[0] == 0 && test_buffer[255] == 0) {
		logbuf_write("[ TEST ] SUCCESS: ");
		logbuf_write(dev->model);
		logbuf_write(" test passed\n");
	} else {
		logbuf_write("[ TEST ] FAILED: clear verification failed.\n");
		retcode = false;
	}

cleanup:
	pma_free_pages(phys_addr, 1);
	return retcode;
}

#endif
