#! /usr/bin/env bash

uv venv --clear
uv sync

prek install --prepare-hooks
