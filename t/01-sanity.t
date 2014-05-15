# vim:set ft= ts=4 sw=4 et:

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(1);

plan tests => repeat_each() * (blocks() * 3);

run_tests();

__DATA__

=== TEST 1: sanity
--- config
    location = /t {
        base64 on;

        echo "hello world";
    }
--- request
    GET /t
--- response_headers
Content-Transfer-Encoding: base64
--- response_body eval: "aGVsbG8gd29ybGQK\r\n"


=== TEST 2: sanity (gzip)
--- config
    location /gzip {
        base64           on;

        gzip             on;
        gzip_min_length  10;
        gzip_types       text/plain;

        echo_duplicate   1000 hello;
    }
--- request
    GET /gzip
--- more_headers
Accept-Encoding: gzip
--- response_headers
Content-Encoding: gzip
Content-Transfer-Encoding: base64


=== TEST 3: base64 max length
--- config
    location = /t {
        base64 on;
        base64_max_length 6;

        content_by_lua '
            ngx.header.content_length = 7
            ngx.say("abcdef")
        ';
    }
--- request
    GET /t
--- response_headers
!Content-Transfer-Encoding
--- response_body
abcdef


=== TEST 4: multiple lines
--- config
    location = /t {
        base64 on;

        echo "nginx [engine x] is an HTTP and reverse proxy server, as well as a mail proxy server, written by Igor Sysoev. For a long time, it has been running on many heavily loaded Russian sites including Yandex, Mail.Ru, VKontakte, and Rambler. According to Netcraft nginx served or proxied 11.48% busiest sites in August 2012. Here are some of the success stories: Netflix, Wordpress.com, FastMail.FM.";
    }
--- request
    GET /t
--- response_headers
Content-Transfer-Encoding: base64
--- response_body eval
"bmdpbnggW2VuZ2luZSB4XSBpcyBhbiBIVFRQIGFuZCByZXZlcnNlIHByb3h5IHNlcnZlciwgYXMg\r\nd2VsbCBhcyBhIG1haWwgcHJveHkgc2VydmVyLCB3cml0dGVuIGJ5IElnb3IgU3lzb2V2LiBGb3Ig\r\nYSBsb25nIHRpbWUsIGl0IGhhcyBiZWVuIHJ1bm5pbmcgb24gbWFueSBoZWF2aWx5IGxvYWRlZCBS\r\ndXNzaWFuIHNpdGVzIGluY2x1ZGluZyBZYW5kZXgsIE1haWwuUnUsIFZLb250YWt0ZSwgYW5kIFJh\r\nbWJsZXIuIEFjY29yZGluZyB0byBOZXRjcmFmdCBuZ2lueCBzZXJ2ZWQgb3IgcHJveGllZCAxMS40\r\nOCUgYnVzaWVzdCBzaXRlcyBpbiBBdWd1c3QgMjAxMi4gSGVyZSBhcmUgc29tZSBvZiB0aGUgc3Vj\r\nY2VzcyBzdG9yaWVzOiBOZXRmbGl4LCBXb3JkcHJlc3MuY29tLCBGYXN0TWFpbC5GTS4K\r\n"
