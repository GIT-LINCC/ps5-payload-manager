#pragma once

/* 
 * Start the autoload sequence in a background thread.
 * Returns 0 on success, -1 on failure.
 */
int nm_autoload_start();

/* 
 * Abort the current autoload sequence if it's in the countdown or sleep phase.
 */
void nm_autoload_abort();
