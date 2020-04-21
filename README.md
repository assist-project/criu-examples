# criu-example
A repository containing examples on using the snapshotting framework CRIU.
A treasure trove of examples is found in CRIU's repository [here][CRIU_EXAMPLES]. 
The [website][CRIU_DOC] documenting CRIU is not entirely up-to-date/complete.
Hence, often times I found answers to my questions by scouring the examples, rather than by searching the website.

## Notes on use
CRIU provides three interfaces, an RPC interface, an command line interface and a C API, libcriu. 
The RPC interface is the main one, with others providing useable adaptors to it.
For our purpose, we focus on libcriu.

More to come...



[CRIU_EXAMPLES]:https://github.com/checkpoint-restore/criu/tree/criu-dev/test/others
[CRIU_DOC]:https://criu.org/Main_Page
