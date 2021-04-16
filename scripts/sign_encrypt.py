#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2015, 2017, 2019, Linaro Limited
#

import sys


algo = {'TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256': 0x70414930,
        'TEE_ALG_RSASSA_PKCS1_V1_5_SHA256': 0x70004830}

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
import os
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


def uuid_parse(s):
    from uuid import UUID
    return UUID(s)


def int_parse(str):
    return int(str, 0)


def get_args(logger):
    from argparse import ArgumentParser, RawDescriptionHelpFormatter
    import textwrap
    command_base = ['sign-enc', 'digest', 'stitch', 'pkcs11-sign']
    command_aliases_digest = ['generate-digest']
    command_aliases_stitch = ['stitch-ta']
    command_aliases = command_aliases_digest + command_aliases_stitch
    command_choices = command_base + command_aliases

    dat = '[' + ', '.join(command_aliases_digest) + ']'
    sat = '[' + ', '.join(command_aliases_stitch) + ']'

    parser = ArgumentParser(
        description='Sign and encrypt (optional) a Tusted Application for' +
        ' OP-TEE.',
        usage='\n   %(prog)s command [ arguments ]\n\n'

        '   command:\n' +
        '     sign-enc    Generate signed and optionally encrypted loadable' +
        ' TA image file.\n' +
        '                 Takes arguments --uuid, --ta-version, --in, --out,' +
        ' --key\n' +
        '                 and --enc-key (optional).\n' +
        '     digest      Generate loadable TA binary image digest' +
        ' for offline\n' +
        '                 signing. Takes arguments --uuid, --ta-version,' +
        ' --in, --key,\n'
        '                 --enc-key (optional), --algo (optional) and' +
        ' --dig.\n' +
        '     stitch      Generate loadable signed and encrypted TA binary' +
        ' image file from\n' +
        '                 TA raw image and its signature. Takes' +
        ' arguments\n' +
        '                 --uuid, --in, --key, --enc-key (optional), --out,' +
        ' --algo (optional) and --sig.\n\n' +
        '     pkcs11-sign      Generate loadable signed TA binary image' +
        'file from TA\n' +
        '                      raw image and using a PKCS#11 token.' +
        'Takes arguments\n' +
        '                 --uuid,  --ta-version, --in and --out.\n\n' +
        '   %(prog)s --help  show available commands and arguments\n\n',
        formatter_class=RawDescriptionHelpFormatter,
        epilog=textwrap.dedent('''\
            If no command is given, the script will default to "sign-enc".

            command aliases:
              The command \'digest\' can be aliased by ''' + dat + '''
              The command \'stitch\' can be aliased by ''' + sat + '\n' + '''
            example offline signing command using OpenSSL for algorithm
            TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256:
              base64 -d <UUID>.dig | \\
              openssl pkeyutl -sign -inkey <KEYFILE>.pem \\
                  -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pss \\
                  -pkeyopt rsa_pss_saltlen:digest \\
                  -pkeyopt rsa_mgf1_md:sha256 | \\
              base64 > <UUID>.sig\n
            example offline signing command using OpenSSL for algorithm
            TEE_ALG_RSASSA_PKCS1_V1_5_SHA256:
              base64 -d <UUID>.dig | \\
              openssl pkeyutl -sign -inkey <KEYFILE>.pem \\
                  -pkeyopt digest:sha256 -pkeyopt rsa_padding_mode:pkcs1 | \\
              base64 > <UUID>.sig
            '''))

    parser.add_argument(
        'command', choices=command_choices, nargs='?',
        default='sign-enc',
        help='Command, one of [' + ', '.join(command_base) + ']')
    parser.add_argument('--uuid', required=True,
                        type=uuid_parse, help='String UUID of the TA')
    parser.add_argument('--key', required=False,
                        help='Name of signing key file (PEM format)')
    parser.add_argument('--enc-key', required=False,
                        help='Encryption key string')
    parser.add_argument(
        '--ta-version', required=False, type=int_parse, default=0,
        help='TA version stored as a 32-bit unsigned integer and used for\n' +
        'rollback protection of TA install in the secure database.\n' +
        'Defaults to 0.')
    parser.add_argument(
        '--sig', required=False, dest='sigf',
        help='Name of signature input file, defaults to <UUID>.sig')
    parser.add_argument(
        '--dig', required=False, dest='digf',
        help='Name of digest output file, defaults to <UUID>.dig')
    parser.add_argument(
        '--in', required=True, dest='inf',
        help='Name of application input file, defaults to <UUID>.stripped.elf')
    parser.add_argument(
        '--out', required=False, dest='outf',
        help='Name of application output file, defaults to <UUID>.ta')
    parser.add_argument('--algo', required=False, choices=list(algo.keys()),
                        default='TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256',
                        help='The hash and signature algorithm, ' +
                        'defaults to TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256. ' +
                        'Allowed values are: ' +
                        ', '.join(list(algo.keys())), metavar='')

    parsed = parser.parse_args()

    # Check parameter combinations

    if parsed.digf is None and \
       parsed.outf is not None and \
       parsed.command in ['digest'] + command_aliases_digest:
        logger.error('A digest was requested, but argument --out was given.' +
                     '  Did you mean:\n  ' +
                     parser.prog+' --dig ' + parsed.outf + ' ...')
        sys.exit(1)

    if parsed.digf is not None \
       and parsed.outf is not None \
       and parsed.command in ['digest'] + command_aliases_digest:
        logger.warn('A digest was requested, but arguments --dig and ' +
                    '--out were given.\n' +
                    '  --out will be ignored.')

    # Set defaults for optional arguments.

    if parsed.sigf is None:
        parsed.sigf = str(parsed.uuid)+'.sig'
    if parsed.digf is None:
        parsed.digf = str(parsed.uuid)+'.dig'
    if parsed.inf is None:
        parsed.inf = str(parsed.uuid)+'.stripped.elf'
    if parsed.outf is None:
        parsed.outf = str(parsed.uuid)+'.ta'

    return parsed


def main():
    try:
        from Cryptodome.Signature import pss
        from Cryptodome.Signature import pkcs1_15
        from Cryptodome.Hash import SHA256
        from Cryptodome.PublicKey import RSA
    except ImportError:
        from Crypto.Signature import pss
        from Crypto.Signature import pkcs1_15
        from Crypto.Hash import SHA256
        from Crypto.PublicKey import RSA
    import base64
    import logging
    import os
    import struct

    logging.basicConfig()
    logger = logging.getLogger(os.path.basename(__file__))

    args = get_args(logger)

    if args.command != 'pkcs11-sign':
        with open(args.key, 'rb') as f:
            key = RSA.importKey(f.read())
            sig_len = key.size_in_bytes()
    else:
        sig_len = 28

    with open(args.inf, 'rb') as f:
        img = f.read()

    h = SHA256.new()

    digest_len = h.digest_size

    img_size = len(img)

    hdr_version = args.ta_version  # struct shdr_bootstrap_ta::ta_version

    magic = 0x4f545348   # SHDR_MAGIC
    if args.enc_key:
        img_type = 2         # SHDR_ENCRYPTED_TA
    else:
        img_type = 1         # SHDR_BOOTSTRAP_TA

    shdr = struct.pack('<IIIIHH',
                       magic, img_type, img_size, algo[args.algo],
                       digest_len, sig_len)
    shdr_uuid = args.uuid.bytes
    shdr_version = struct.pack('<I', hdr_version)

    if args.enc_key:
        from Cryptodome.Cipher import AES
        cipher = AES.new(bytearray.fromhex(args.enc_key), AES.MODE_GCM)
        ciphertext, tag = cipher.encrypt_and_digest(img)

        enc_algo = 0x40000810  # TEE_ALG_AES_GCM
        flags = 0              # SHDR_ENC_KEY_DEV_SPECIFIC
        ehdr = struct.pack('<IIHH',
                           enc_algo, flags, len(cipher.nonce), len(tag))

    h.update(shdr)
    h.update(shdr_uuid)
    h.update(shdr_version)
    if args.enc_key:
        h.update(ehdr)
        h.update(cipher.nonce)
        h.update(tag)
    h.update(img)
    img_digest = h.digest()

    def write_image_with_signature(sig):
        with open(args.outf, 'wb') as f:
            f.write(shdr)
            f.write(img_digest)
            f.write(sig)
            f.write(shdr_uuid)
            f.write(shdr_version)
            if args.enc_key:
                f.write(ehdr)
                f.write(cipher.nonce)
                f.write(tag)
                f.write(ciphertext)
            else:
                f.write(img)

    def sign_encrypt_ta():
        if not key.has_private():
            logger.error('Provided key cannot be used for signing, ' +
                         'please use offline-signing mode.')
            sys.exit(1)
        else:
            signer = pss.new(key)
            sig = signer.sign(h)
            if len(sig) != sig_len and args.command != 'pkcs11-sign':
                raise Exception(("Actual signature length is not equal to ",
                                 "the computed one: {} != {}").
                                format(len(sig), sig_len))
            write_image_with_signature(sig)
            logger.info('Successfully signed application.')

    def pkcs11_sign_ta():
        import pkcs11

        lib = pkcs11.lib(os.environ['PKCS11_MODULE'])
        token = lib.get_token(token_label=pkcs11_token_label)
        with token.open(user_pin=pkcs11_token_pin) as session:
            key = session.get_key(key_type=pkcs11.KeyType.RSA,
                                  object_class=pkcs11.ObjectClass.PRIVATE_KEY,
                                  label=pkcs11_key_label,
                                  id=pkcs11_key_id)

            if args.algo == 'TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256':
                mecha = pkcs11.Mechanism.RSA_PKCS_PSS
                param = (pkcs11.Mechanism.SHA256, pkcs11.MGF.SHA1, 0)
            elif args.algo == 'TEE_ALG_RSASSA_PKCS1_V1_5_SHA256':
                mecha = pkcs11.Mechanism.RSA_PKCS
                param = None

            sig = key.sign(img_digest, mechanism=mecha, mechanism_param=param)
            write_image_with_signature(sig)
            logger.info('Successfully signed application w/ PKCS#11 token')

    def generate_digest():
        with open(args.digf, 'wb+') as digfile:
            digfile.write(base64.b64encode(img_digest))

    def stitch_ta():
        try:
            with open(args.sigf, 'r') as sigfile:
                sig = base64.b64decode(sigfile.read())
        except IOError:
            if not os.path.exists(args.digf):
                generate_digest()
            logger.error('No signature file found. Please sign\n %s\n' +
                         'offline and place the signature at \n %s\n' +
                         'or pass a different location ' +
                         'using the --sig argument.\n',
                         args.digf, args.sigf)
            sys.exit(1)
        else:
            if args.algo == 'TEE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256':
                verifier = pss.new(key)
            elif args.algo == 'TEE_ALG_RSASSA_PKCS1_V1_5_SHA256':
                verifier = pkcs1_15.new(key)
            try:
                verifier.verify(h, sig)
                write_image_with_signature(sig)
                logger.info('Successfully applied signature.')
            except ValueError:
                logger.error('Verification failed, ignoring given signature.')
                sys.exit(1)

    # dispatch command
    {
        'sign-enc': sign_encrypt_ta,
        'digest': generate_digest,
        'generate-digest': generate_digest,
        'stitch': stitch_ta,
        'stitch-ta': stitch_ta,
        'pkcs11-sign': pkcs11_sign_ta
    }.get(args.command, 'sign_encrypt_ta')()


if __name__ == "__main__":
    main()
