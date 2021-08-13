# syntax = edrevo/dockerfile-plus

FROM alpine:3.13 AS robotv-vscode

INCLUDE+ docker/includes/cpu.x86_64
INCLUDE+ docker/includes/robotv.env
INCLUDE+ docker/includes/robotv.build

# RUN [ ! -f .devcontainer/env ] && cp .devcontainer/env.template .devcontainer/env
