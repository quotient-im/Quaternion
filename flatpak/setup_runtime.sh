#!/usr/bin/env bash

flatpak --user remote-add flathub --if-not-exists --from https://flathub.org/repo/flathub.flatpakrepo
flatpak --user install flathub org.kde.Platform//5.9
flatpak --user install flathub org.kde.Sdk//5.9