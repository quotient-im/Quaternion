# Security Policy

## Supported Versions

Only the latest stable (i.e. not beta) version of Quaternion is supported with security updates.
Also, an effort is put into supporting the version on most recent stable releases of each major
Linux distribution (Debian, Ubuntu, Fedora, OpenSuse). Users of older Quaternion versions are
strongly advised to upgrade to the latest release - support of those versions is very limited,
if provided at all. If you can't do it because your Linux distribution is too old, you likely
have other security problems as well; upgrade your Linux distribution!

## Reporting a Vulnerability

If you find a significant vulnerability, or evidence of one, use either of the following contacts:
- send an email to [Kitsune Ral](mailto:Kitsune-Ral@users.sf.net); or
- reach out in Matrix to [@kitsune:matrix.org](https://matrix.to/#/@kitsune:matrix.org) (if you can, switch encryption on).

In any of these two options, first indicate that you have such information
(do not disclose it yet) and wait for further instructions.

By default, we will give credit to anyone who reports a vulnerability in a responsible way so that we can fix it before public disclosure.
If you want to remain anonymous or pseudonymous instead, please let us know; we will gladly respect your wishes.

If you provide a security fix as a PR, you have no way to remain anonymous; you
also thereby lay out the vulnerability itself so this is NOT the right way for
undisclosed vulnerabilities, whether or not you want to stay incognito.

## Timeline and commitments

Initial reaction to the message about a vulnerability (see above) will be no more than 5 days. From the moment of the private report or
public disclosure (if it hasn't been reported earlier in private) of each vulnerability, we take effort to fix it on priority before
any other issues. In case of vulnerabilities with [CVSS v2](https://nvd.nist.gov/cvss.cfm) score of 4.0 and higher the commitment is
to provide a workaround within 30 days and a full fix within 60 days after the specific information on the vulnerability has been
reported to the project by any means (in private or in public). For vulnerabilities with lower score there is no commitment on the timeline,
only prioritisation. The full fix doesn't imply that all software functionality remains accessible (in the worst case
the vulnerable functionality may be disabled or removed to prevent the attack).
