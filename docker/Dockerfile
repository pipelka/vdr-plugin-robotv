# syntax = edrevo/dockerfile-plus

FROM alpine:3.13 AS robotv-build

INCLUDE+ docker/includes/cpu.x86_64
INCLUDE+ docker/includes/robotv.build

FROM alpine:3.13 AS robotv

INCLUDE+ docker/includes/cpu.x86_64
INCLUDE+ docker/includes/robotv.env
INCLUDE+ docker/includes/robotv.image
