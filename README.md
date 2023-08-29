1. Describe how your exchange works.
The exchange stores orders as objects, keeping the user, user order id, time (via global id), and so on. It is stored such that the following three:
- Access (meaning add, change, remove) order given trader and order id
- Access min-sell and max-buy for a particular product
- Access all orders for product in sorted order
Are quick (tree).
When a message is received, the exchange first checks for message validity (works for malformed input too by reading one character at a time) then depending on the command, does the operation by manipulating the order trees. To be convenient, the orders themselves are not stored in a the tree but a reference, such that changes across a single order is globally visible.
The exchange uses the self-pipe trick to handle USR1 signals to reduce misses. This way I can monitor for incoming signals and pipe breaks with one multiplexer.

2. Describe your design decisions for the trader and how it's fault-tolerant.
The exchange can miss a signal and thus does not send a confirming message. Because the message is still in the pipe, all the trader has to do is to resend the signal. I am saving the pending orders and deleting them when confirmed (as opposed to a counter) to accomodate for that fact that the exchange could send an invalid accept message. To not place too much load on the exchange I only send a signal after a timeout of 2 seconds, and check for timeouts every second (signals make sleep end anyway so they are caught on time). The same input validation on the exchange is done in the trader too to handle errors gracefully.

3. Describe your tests and how to run them.
E2E testing consists of:
- A test-writer that reads a file and writes the messages to the pipe
- A test-reader that accepts global messages
- The modified exchange that initialise multiple instances of test-writers and send them the correct info (controlled via proprocessor)
Unit testing tests the read function to see that it handles many inputs.
