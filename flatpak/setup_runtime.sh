#!/usr/bin/env bash

flatpak --user remote-add kderuntime --if-not-exists --from https://distribute.kde.org/kderuntime.flatpakrepo
flatpak --user install kderuntime org.kde.Platform
flatpak --user install kderuntime org.kde.Sdk
