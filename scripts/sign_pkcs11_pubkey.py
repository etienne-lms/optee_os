#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021, Linaro Limited
#

from asn1crypto import pem
import base64
import logging
import os
import pkcs11
import sys


# Set PKCS#11 token and key references/arguments
#
# Token label is mandatory. Default value: see default_pkcs11_token_label.
# Can be set from environmental variable PKCS11_TOKEN_LABEL.
#
# Token user PIN is optional, currently defaults to None.
# Can be set from environmental variable PKCS11_TOKEN_PIN.
# If None, the token is not logged in.
#
# Key is searched based on expected key type and optionally its
# label and/or its ID. Currently defaults to None (not used
# find target key in the PKCS#11 token).
# Can be set from environmental variables PKCS11_KEY_LABEL and
# PKCS11_KEY_ID.
#
default_pkcs11_token_label = 'OP-TEE TA authentication'
default_pkcs11_token_pin = None
default_pkcs11_key_label = None
default_pkcs11_key_id = None
try:
    pkcs11_token_label = os.environ['PKCS11_TOKEN_LABEL']
except:
    pkcs11_token_label = default_pkcs11_token_label
try:
    pkcs11_token_pin = os.environ['PKCS11_TOKEN_PIN']
except:
    pkcs11_token_pin = default_pkcs11_token_pin
try:
    pkcs11_key_label = os.environ['PKCS11_KEY_LABEL']
except:
    pkcs11_key_label = default_pkcs11_key_label
try:
    pkcs11_key_id = os.environ['PKCS11_KEY_ID']
except:
    pkcs11_key_id = default_pkcs11_key_id
else:
    pkcs11_key_id = bytes.fromhex(pkcs11_key_id)


def get_args(logger):
    from argparse import ArgumentParser, RawDescriptionHelpFormatter
    import textwrap

    parser = ArgumentParser(
        description='Retrieve TA authentication public key from PKCS#11 token',
        usage='\n   %(prog)s --out PEM-FILE\n\n'
         '   %(prog)s --help  show available commands and arguments\n\n',
        formatter_class=RawDescriptionHelpFormatter)

    parser.add_argument(
        '--out', required=False, dest='outf',
        help='Name of public key PEM output file, defaults to pubkey.pem')

    parsed = parser.parse_args()

    # Set defaults for optional arguments.

    if parsed.outf is None:
        parsed.outf = 'pubkey.pem'

    return parsed


def main():
    logging.basicConfig()
    logger = logging.getLogger(os.path.basename(__file__))

    args = get_args(logger)

    lib = pkcs11.lib(os.environ['PKCS11_MODULE'])
    token = lib.get_token(token_label=pkcs11_token_label)
    with token.open(user_pin=pkcs11_token_pin) as session:
        from pkcs11.util.rsa import encode_rsa_public_key

        pub = session.get_key(key_type=pkcs11.KeyType.RSA,
                              object_class=pkcs11.ObjectClass.PUBLIC_KEY,
                              label=pkcs11_key_label,
                              id=pkcs11_key_id)
        pub = pem.armor('RSA PUBLIC KEY', encode_rsa_public_key(pub))
        with open(args.outf, 'wb') as f:
            f.write(pub)


if __name__ == "__main__":
    main()
