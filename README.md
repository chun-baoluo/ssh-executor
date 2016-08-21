# ssh-executor

Simple ssh executor for Node.js written in C++ using libssh2. Returns terminal output after successful command execution. Currently works on Linux only.

## Installation

You need to have libssh2 installed on your computer:

```sh
$ sudo apt-get install libssh2-1-dev
```

Also, make sure Node-gyp module is installed (in order to compile C++ code into a Node addon):

```sh
$ npm install -g node-gyp
```

Then you can proceed with npm installation:

```sh
$ npm install ssh-executor
```

## Usage

```sh
const ssh = require('ssh-executor');

ssh.connect({ // Establish connection
	host: "127.0.0.1",
	port: '22',
	username: "yourusername",
	password: 'yourpassword'
}, (err) => {
	if(!err) {
		console.log('Successfully connected!');
	}
});

ssh.exec('echo NOBODY EXPECTS THE SPANISH INQUISITION!', (result, err) => {
	if(!err) {
		console.log(result); //NOBODY EXPECTS THE SPANISH INQUISITION!
	}
}); // Execute your command

ssh.close(); // Close your SSH connection.
```