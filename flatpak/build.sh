#!/usr/bin/env bash

flatpak-builder --ccache --force-clean --require-changes --repo=repo --subject="Nightly build of Quaternion, `date`" ${EXPORT_ARGS-} app io.github.quotient_im.Quaternion.yaml

flatpak --user remote-add --if-not-exists quaternion-nightly repo/ --no-gpg-verify
