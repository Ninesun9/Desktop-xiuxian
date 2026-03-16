#!/usr/bin/env python
# -*- coding: utf-8 -*-

import base64
import datetime
import hmac
import json
import os
import re

from flask import Flask, jsonify, request
import MySQLdb
from MySQLdb import DatabaseError
from HTMLTable import HTMLTable
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad


app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = int(os.environ.get('XIUXIAN_MAX_CONTENT_LENGTH', 16 * 1024))


def find_chinese(file):
    pattern = re.compile(r'[^\u4e00-\u9fa5]')
    chinese = re.sub(pattern, '', file)
    print(chinese)


def find_unchinese(file):
    pattern = re.compile(r'[\u4e00-\u9fa5]')
    unchinese = re.sub(pattern, '', file)
    print(unchinese)


def ftos(value):
    return '{:3.2g}'.format(value)


def _env(name, default=None, required=False):
    value = os.environ.get(name, default)
    if required and (value is None or value == ''):
        raise RuntimeError('missing required environment variable: {}'.format(name))
    return value


def _db_config():
    return {
        'host': _env('XIUXIAN_DB_HOST', '127.0.0.1'),
        'user': _env('XIUXIAN_DB_USER', required=True),
        'passwd': _env('XIUXIAN_DB_PASSWORD', required=True),
        'db': _env('XIUXIAN_DB_NAME', 'xiuxian'),
        'charset': 'utf8mb4',
        'port': int(_env('XIUXIAN_DB_PORT', '3306')),
    }


def get_db_connection():
    return MySQLdb.connect(**_db_config())


def execute_write(sql, params):
    db = get_db_connection()
    cursor = db.cursor()
    try:
        cursor.execute(sql, params)
        db.commit()
    except DatabaseError:
        db.rollback()
        app.logger.exception('database write failed')
        raise
    finally:
        cursor.close()
        db.close()


def fetch_rows(sql, params=None):
    db = get_db_connection()
    cursor = db.cursor()
    try:
        cursor.execute(sql, params or ())
        return cursor.fetchall()
    finally:
        cursor.close()
        db.close()


def json_error(message, status_code):
    response = jsonify({'result': 'error', 'message': message})
    response.status_code = status_code
    return response


def require_api_token():
    expected = _env('XIUXIAN_API_TOKEN', required=True)
    provided = request.headers.get('X-Xiuxian-Token', '')
    return hmac.compare_digest(provided, expected)


def get_cipher_key():
    key = _env('XIUXIAN_SHARED_KEY', required=True).encode('utf-8')
    if len(key) not in (16, 24, 32):
        raise RuntimeError('XIUXIAN_SHARED_KEY must be 16, 24, or 32 bytes long')
    return key


def decrypt_payload(encoded_text):
    try:
        encrypted = base64.b64decode(encoded_text, validate=True)
    except (TypeError, ValueError):
        raise ValueError('payload is not valid base64')

    if len(encrypted) <= AES.block_size or len(encrypted) % AES.block_size != 0:
        raise ValueError('payload size is invalid')

    iv = encrypted[:AES.block_size]
    ciphertext = encrypted[AES.block_size:]
    cipher = AES.new(get_cipher_key(), AES.MODE_CBC, iv=iv)

    try:
        plain_text = unpad(cipher.decrypt(ciphertext), AES.block_size)
    except ValueError:
        raise ValueError('payload decryption failed')

    try:
        return json.loads(plain_text.decode('utf-8'))
    except (TypeError, ValueError, UnicodeDecodeError):
        raise ValueError('payload is not valid json')


def coerce_string(payload, field_name, max_length=128):
    value = payload.get(field_name)
    if not isinstance(value, str):
        raise ValueError('{} must be a string'.format(field_name))
    value = value.strip()
    if not value:
        raise ValueError('{} cannot be empty'.format(field_name))
    return value[:max_length]


def coerce_float(payload, field_name):
    try:
        return float(payload.get(field_name))
    except (TypeError, ValueError):
        raise ValueError('{} must be a number'.format(field_name))


def coerce_int(payload, field_name):
    try:
        return int(payload.get(field_name))
    except (TypeError, ValueError):
        raise ValueError('{} must be an integer'.format(field_name))


def parse_encrypted_request():
    if not require_api_token():
        return None, json_error('unauthorized', 401)

    raw_body = request.get_data(as_text=True)
    if not raw_body:
        return None, json_error('request body is empty', 400)

    try:
        payload = decrypt_payload(raw_body)
    except RuntimeError as exc:
        app.logger.exception('security configuration error')
        return None, json_error(str(exc), 500)
    except ValueError as exc:
        return None, json_error(str(exc), 400)

    if not isinstance(payload, dict):
        return None, json_error('payload must be a json object', 400)

    return payload, None


def render_rank_table(headers, rows):
    table = HTMLTable(caption='')
    table.append_data_rows((headers,))
    for row in rows:
        table.append_data_rows((row,))
    table.set_cell_style({
        'border-color': '#000',
        'border-width': '1px',
        'border-style': 'solid',
        'padding': '5px',
    })
    return table.to_html()


@app.errorhandler(413)
def handle_payload_too_large(_error):
    return json_error('payload too large', 413)


@app.route('/updata-new', methods=['POST'])
def add_task():
    payload, error = parse_encrypted_request()
    if error:
        return error

    try:
        params = (
            coerce_string(payload, 'userid', 64),
            coerce_string(payload, 'username', 6),
            coerce_float(payload, 'shengming'),
            coerce_float(payload, 'gongji'),
            coerce_float(payload, 'fangyu'),
            coerce_float(payload, 'wuxing'),
            coerce_float(payload, 'xiuwei'),
            coerce_int(payload, 'jingjie'),
            str(request.remote_addr or ''),
            datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        )
    except ValueError as exc:
        return json_error(str(exc), 400)

    sql = (
        'REPLACE INTO xiuxian '
        '(uuid, user_name, sheng_ming, gong_ji, fang_yu, wu_xing, xiu_wei, jing_jie, remote_addr, up_dt) '
        'VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)'
    )

    try:
        execute_write(sql, params)
    except RuntimeError as exc:
        app.logger.exception('security configuration error')
        return json_error(str(exc), 500)
    except DatabaseError:
        return json_error('database write failed', 500)

    return jsonify({'result': 'success'})


@app.route('/updata-chuangdangjianghu', methods=['POST'])
def add_chuangdang():
    payload, error = parse_encrypted_request()
    if error:
        return error

    try:
        params = (
            coerce_string(payload, 'userid', 64),
            coerce_string(payload, 'uname', 6),
            coerce_int(payload, 'jianggong'),
            coerce_int(payload, 'jifen'),
            datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        )
    except ValueError as exc:
        return json_error(str(exc), 400)

    sql = 'INSERT INTO chuangdang (uuid, uname, jianggong, jifen, updt) VALUES (%s, %s, %s, %s, %s)'

    try:
        execute_write(sql, params)
    except RuntimeError as exc:
        app.logger.exception('security configuration error')
        return json_error(str(exc), 500)
    except DatabaseError:
        return json_error('database write failed', 500)

    return jsonify({'result': 'success'})


@app.route('/updata', methods=['GET'])
def get_updata():
    return '这个服务是使用Python编写运行在树莓派上的，脆弱得很，请大佬高抬贵手'


@app.route('/', methods=['GET'])
def get_root():
    return '这个服务是使用Python编写运行在树莓派上的，脆弱得很，请大佬高抬贵手'


@app.route('/feedback', methods=['POST'])
def post_feedback():
    if not require_api_token():
        return json_error('unauthorized', 401)

    data = request.get_data(as_text=True)
    if not data:
        return json_error('feedback cannot be empty', 400)

    info = data.strip()[:1000]
    if not info:
        return json_error('feedback cannot be empty', 400)

    sql = 'REPLACE INTO feedback (info, up_dt) VALUES (%s, %s)'
    params = (info, datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'))

    try:
        execute_write(sql, params)
    except RuntimeError as exc:
        app.logger.exception('security configuration error')
        return json_error(str(exc), 500)
    except DatabaseError:
        return json_error('database write failed', 500)

    return jsonify({'result': 'success'})


@app.route('/lastVersion', methods=['GET'])
def get_version():
    return '''1.5.1.0|https://www.52pojie.cn/thread-1498266-1-1.html|
    <h1>开启全新资料片【闯荡江湖】</h1><br>
    1、擂台积分排名上线<br>
    2、大家可以更和谐地论剑了

    '''


@app.route('/rank/xiuwei', methods=['GET'])
def get_rank_xiuwei():
    sql = """
        SELECT user_name, sheng_ming, gong_ji, fang_yu, xiu_wei, jingjie_str
        FROM `xiuxian` a inner join `dict_jingjie` b on a.jing_jie = b.jingjie_id
        WHERE LENGTH(user_name) > 1
            AND gong_ji < 1e30
            AND fang_yu < 1e30
            AND xiu_wei < 4.87934578929732e29
            AND HOUR(timediff(now(), up_dt)) < 24
            AND id IN (select max(id) from xiuxian group by uuid)
            AND sheng_ming <> gong_ji
            AND gong_ji <> fang_yu
            AND fang_yu <> xiu_wei
            AND xiu_wei <> sheng_ming
            AND fang_yu <> sheng_ming
        ORDER BY xiu_wei desc LIMIT 0,20;
    """
    results = fetch_rows(sql)
    rows = []
    for index, row in enumerate(results, start=1):
        rows.append((str(index), str(row[0]), str(row[1]), str(row[2]), str(row[3]), str(row[4]), str(row[5])))
    return render_rank_table(('排名', '道号', '生命', '攻击', '防御', '修为', '境界'), rows)


@app.route('/rank/shengming', methods=['GET'])
def get_rank_shengming():
    sql = """
        SELECT user_name, sheng_ming, gong_ji, fang_yu, xiu_wei, jingjie_str
        FROM `xiuxian` a join `dict_jingjie` b on a.jing_jie = b.jingjie_id
        WHERE LENGTH(user_name) > 1
            AND sheng_ming < 1e30
            AND gong_ji < 1e30
            AND fang_yu < 1e30
            AND xiu_wei < 4.87934578929732e29
            AND HOUR(timediff(now(), up_dt)) < 24
            AND id IN (select max(id) from xiuxian group by uuid)
            AND sheng_ming <> gong_ji
            AND gong_ji <> fang_yu
            AND fang_yu <> xiu_wei
            AND xiu_wei <> sheng_ming
            AND fang_yu <> sheng_ming
        ORDER BY CONVERT(sheng_ming, SIGNED) desc LIMIT 0,20;
    """
    results = fetch_rows(sql)
    rows = []
    for row in results:
        rows.append((str(row[0]), str(row[1]), str(row[2]), str(row[3]), str(row[4]), str(row[5])))
    return render_rank_table(('道号', '生命', '攻击', '防御', '修为', '境界'), rows)


@app.route('/rank/gongji', methods=['GET'])
def get_rank_gongji():
    sql = """
        SELECT user_name, sheng_ming, gong_ji, fang_yu, xiu_wei, jingjie_str
        FROM `xiuxian` a join `dict_jingjie` b on a.jing_jie = b.jingjie_id
        WHERE LENGTH(user_name) > 1
            AND sheng_ming < 1e30
            AND gong_ji < 1e30
            AND fang_yu < 1e30
            AND xiu_wei < 4.87934578929732e29
            AND HOUR(timediff(now(), up_dt)) < 24
            AND id IN (select max(id) from xiuxian group by uuid)
            AND sheng_ming <> gong_ji
            AND gong_ji <> fang_yu
            AND fang_yu <> xiu_wei
            AND xiu_wei <> sheng_ming
            AND fang_yu <> sheng_ming
        ORDER BY CONVERT(gong_ji, SIGNED) desc LIMIT 0,20;
    """
    results = fetch_rows(sql)
    rows = []
    for row in results:
        rows.append((str(row[0]), str(row[1]), str(row[2]), str(row[3]), str(row[4]), str(row[5])))
    return render_rank_table(('道号', '生命', '攻击', '防御', '修为', '境界'), rows)


@app.route('/rank/fangyu', methods=['GET'])
def get_rank_fangyu():
    sql = """
        SELECT user_name, sheng_ming, gong_ji, fang_yu, xiu_wei, jingjie_str
        FROM `xiuxian` a join `dict_jingjie` b on a.jing_jie = b.jingjie_id
        WHERE LENGTH(user_name) > 1
            AND sheng_ming < 1e30
            AND gong_ji < 1e30
            AND fang_yu < 1e30
            AND xiu_wei < 4.87934578929732e29
            AND HOUR(timediff(now(), up_dt)) < 24
            AND id IN (select max(id) from xiuxian group by uuid)
            AND sheng_ming <> gong_ji
            AND gong_ji <> fang_yu
            AND fang_yu <> xiu_wei
            AND xiu_wei <> sheng_ming
            AND fang_yu <> sheng_ming
        ORDER BY CONVERT(fang_yu, SIGNED) desc LIMIT 0,20;
    """
    results = fetch_rows(sql)
    rows = []
    for row in results:
        rows.append((str(row[0]), str(row[1]), str(row[2]), str(row[3]), str(row[4]), str(row[5])))
    return render_rank_table(('道号', '生命', '攻击', '防御', '修为', '境界'), rows)


@app.route('/rank/jifen', methods=['GET'])
def get_rank_jifen():
    sql = """
        SELECT uname, jianggong, jifen
        FROM `chuangdang`
        WHERE HOUR(timediff(now(), updt)) < 24
            AND ziznegid IN (select max(ziznegid) from chuangdang group by uuid)
        ORDER BY jifen desc LIMIT 0,200;
    """
    results = fetch_rows(sql)
    rows = []
    for index, row in enumerate(results, start=1):
        rows.append((str(index), str(row[0]), str(row[2])))
    return render_rank_table(('排名', '道号', '积分'), rows)


if __name__ == '__main__':
    app.run(
        host=_env('XIUXIAN_SERVER_HOST', '127.0.0.1'),
        port=int(_env('XIUXIAN_SERVER_PORT', '8522')),
    )

