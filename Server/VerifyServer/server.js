const crypto = require('crypto');
const express = require('express');

const { isMockMode, sendVerifyCode } = require('./email');

// 与 GateServer 的 ErrorCodes 保持一致的业务错误码。
const ErrorCode = {
    Success: 0,
    InvalidParam: 1001,
    SendFailed: 1002,
};

const HOST = process.env.HOST || '127.0.0.1';
const PORT = Number.parseInt(process.env.PORT || '50051', 10);
const CODE_TTL_SECONDS = Number.parseInt(process.env.CODE_TTL_SECONDS || '300', 10);

if (!Number.isInteger(PORT) || PORT <= 0 || PORT > 65535) {
    console.error(`invalid PORT: ${process.env.PORT}`);
    process.exit(1);
}

if (!Number.isInteger(CODE_TTL_SECONDS) || CODE_TTL_SECONDS <= 0) {
    console.error(`invalid CODE_TTL_SECONDS: ${process.env.CODE_TTL_SECONDS}`);
    process.exit(1);
}

// 先用内存保存验证码，接入 Redis 后替换这部分即可。
// key: email, value: { code, expiresAt }
const codeStore = new Map();

function isValidEmail(email) {
    return typeof email === 'string' && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

// 生成 4 位数字验证码，范围 1000 ~ 9999。
function generateCode() {
    return crypto.randomInt(1000, 10000).toString();
}

function getValidCode(email) {
    const item = codeStore.get(email);
    if (!item) {
        return null;
    }

    if (item.expiresAt <= Date.now()) {
        codeStore.delete(email);
        return null;
    }

    return item.code;
}

function saveCode(email, code) {
    codeStore.set(email, {
        code,
        expiresAt: Date.now() + CODE_TTL_SECONDS * 1000,
    });
}

// 定期清理过期验证码，避免 Map 长期增长。
// unref() 保证这个定时器不会阻止进程退出。
setInterval(() => {
    const now = Date.now();
    for (const [email, item] of codeStore) {
        if (item.expiresAt <= now) {
            codeStore.delete(email);
        }
    }
}, 60 * 1000).unref();//如果主程序退出了，别管这个定时器，让程序正常死掉

const app = express();

// 限制请求体大小，验证码接口只需要一个 email 字段。
app.use(express.json({ limit: '16kb' }));

app.post('/get_varifycode', async (req, res) => {
    const email = req.body ? req.body.email : undefined;

    if (!isValidEmail(email)) {
        return res.json({
            error: ErrorCode.InvalidParam,
            email: typeof email === 'string' ? email : '',
        });
    }

    // 如果同一个邮箱已有未过期验证码，则复用，避免用户连续点击时收到多个不同验证码。
    let code = getValidCode(email);
    if (!code) {
        code = generateCode();
        saveCode(email, code);
    }

    try {
        await sendVerifyCode(email, code);
        console.log(`[VerifyServer] code sent to ${email}${isMockMode() ? ' (mock)' : ''}`);
        return res.json({
            error: ErrorCode.Success,
            email,
        });
    } catch (error) {
        // 发送失败时删除验证码，避免留下一个用户永远收不到的 code。
        codeStore.delete(email);
        console.error(`[VerifyServer] send code to ${email} failed: ${error.message}`);
        return res.json({
            error: ErrorCode.SendFailed,
            email,
        });
    }
});

// 未匹配的路由统一返回 JSON，避免返回 HTML 让调用方解析失败。
app.use((req, res) => {
    res.status(404).json({
        error: ErrorCode.InvalidParam,
        message: 'not found',
    });
});

// 处理 express.json() 解析非法 JSON 等异常。
// 业务接口统一返回 HTTP 200 + error 字段，方便 GateServer 统一处理。
app.use((error, req, res, next) => {
    if (error && error.type === 'entity.parse.failed') {
        return res.json({ error: ErrorCode.InvalidParam });
    }

    console.error(`[VerifyServer] unhandled error: ${error.message}`);
    return res.status(500).json({ error: ErrorCode.SendFailed });
});

const server = app.listen(PORT, HOST, () => {
    const mockText = isMockMode() ? ' [mock email]' : '';
    console.log(`VerifyServer listening on http://${HOST}:${PORT}${mockText}`);
});

server.on('error', (error) => {
    console.error(`[VerifyServer] listen failed: ${error.message}`);
    process.exit(1);
});

// 支持 Ctrl+C / systemd 停止时优雅退出。
function shutdown(signal) {
    console.log(`[VerifyServer] received ${signal}, shutting down`);
    server.close(() => {
        process.exit(0);
    });
}

process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));
