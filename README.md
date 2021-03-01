# idrivebmr-server

IDriveBMR main server source code

### Steps to create the server binary

* Clone the repo to any IDrive bmr server as it already contains all the dependency for the code to compile.

* Run ./configure to generate makefile.Make sure prefix and localstatedir variables are set to /usr and /var respectively if not.

* Run make to generate idrivebmrsrv binary in root directory along with zfs and snapshot helper.

* make sure binary permission is set to 755 before running the binary.