FROM php:7.1-fpm

ADD sendmail.c /root/

RUN gcc -Wall -Werror -static /root/sendmail.c -o /usr/local/sbin/sendmail