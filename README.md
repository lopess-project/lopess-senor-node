# lopess-senor-node

Directory Structure:
* esp-idf: remote repository with all esp-idf related components
* lopy4: main working directory. Within this directory, execute `make flash` in order to flash your lopy
* main_files: contains all related main routines which were used for testing
* xtensa: necessary for compiling the project

NOTE:
Some main files contain public and private key information which have been used for testing. Do NOT use this key material in a productional environment. Generate new key material and update the public keys in the backend accordingly.
