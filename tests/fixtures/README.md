# Project-owned encrypted PDF fixtures

Generated from scripts/make_fixture.py using scripts/make_security_fixtures.py
(pypdf AES-128). These are test documents, not third-party dependency binaries.
Their public owner password is `test-owner`. The `password.pdf` user password is
`test-user`; other fixtures use an empty user password. Regeneration uses randomized
encryption IDs, so new bytes require review and refreshed hashes. Normal builds
use the bundled fixtures and do not require pypdf or cryptography.

restricted.pdf deliberately contains a form while forbidding printing/editing/copy.
Printing, modification, extraction and form permissions are independent. These
fixtures exercise MuPDF independently of its own PDF-writing implementation.
