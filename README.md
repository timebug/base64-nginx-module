Name
====

**ngx_base64** - Nginx HTTP Base64 Filter Module.

*This module is not distributed with the Nginx source.* See [the installation instructions](#installation).

Table of Contents
=================

* [Name](#name)
* [Status](#status)
* [Synopsis](#synopsis)
* [Description](#description)
* [Directives](#directives)
    * [base64](#base64)
    * [base64_max_length](#base64_max_length)
* [Installation](#installation)
* [Author](#author)
* [Copyright and License](#copyright-and-license)
* [See Also](#see-also)

Status
======

This module is already quite usable though still at the early phase of development
and is considered experimental.

Synopsis
========

```nginx
    location = /t {
        base64 on;
        base64_max_length 10485760;

        echo "hello world";
    }
```

Description
===========

This module allows for on-the-fly base64 encode. As same as the standard [ngx_http_gzip_module](http://nginx.org/en/docs/http/ngx_http_gzip_module.html).

[Back to TOC](#table-of-contents)

Directives
==========

[Back to TOC](#table-of-contents)

base64
--------------
**syntax:** *base64 on | off*

**default:** *off*

**context:** *http, server, location, location if*

Enables or disables base64 encode.

base64_max_length
--------------
**syntax:** *base64_max_length &lt;length&gt;*

**default:** *1048576 (1MB)*

**context:** *http, server, location*

Sets the maximum length, in bytes, of the response that will be encode. Responses larger than this byte-length will not be encode. Length is determined from the "Content-Length" header.

[Back to TOC](#table-of-contents)

Installation
============

Grab the nginx source code from [nginx.org](http://nginx.org/), for example,
the version 1.10.0, and then build the source with this module:

```bash

$ wget 'http://nginx.org/download/nginx-1.10.0.tar.gz'
$ tar -xzvf nginx-1.10.0.tar.gz
$ cd nginx-1.10.0/

# Here we assume you would install you nginx under /opt/nginx/.
$ ./configure --prefix=/opt/nginx \
    --add-module=/path/to/base64-nginx-module

$ make -j2
$ make install
```

[Back to TOC](#table-of-contents)

Author
======

Monkey Zhang (timebug) <timebug.info@gmail.com>, UPYUN Inc.

[Back to TOC](#table-of-contents)

Copyright and License
=====================

This module is licensed under the BSD license.

Copyright (C) 2014 - 2017, by Monkey Zhang (timebug), UPYUN Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

[Back to TOC](#table-of-contents)
