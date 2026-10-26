const nodemailer = require('nodemailer');

// 是否使用 mock 邮件模式。
// mock 模式不会连接 SMTP，只在控制台打印验证码，方便本地联调。
function isMockMode() {
    return process.env.MOCK_EMAIL === '1';
}

let transporter = null;

function getTransporter() {
    if (transporter) {
        return transporter;
    }

    const host = process.env.SMTP_HOST;
    const port = Number.parseInt(process.env.SMTP_PORT || '465', 10);
    const secure = process.env.SMTP_SECURE !== '0';
    const user = process.env.SMTP_USER;
    const pass = process.env.SMTP_PASS;

    if (!host || !user || !pass) {
        throw new Error('SMTP_HOST / SMTP_USER / SMTP_PASS is not configured');
    }

    // 163 邮箱一般使用 465 端口 + SSL。
    // 超时设置避免 SMTP 异常时请求一直挂起。
    transporter = nodemailer.createTransport({
        host,
        port,
        secure,
        auth: {
            user,
            pass,
        },
        connectionTimeout: 10000,
        greetingTimeout: 10000,
        socketTimeout: 10000,
    });

    return transporter;
}

async function sendVerifyCode(email, code) {
    if (isMockMode()) {
        // 只打印验证码，不发送真实邮件。
        console.log(`[MOCK_EMAIL] to=${email} code=${code}`);
        return;
    }

    const client = getTransporter();
    const from = process.env.SMTP_FROM || process.env.SMTP_USER;

    await client.sendMail({
        from,
        to: email,
        subject: 'OpenChat 验证码',
        text: `您的验证码为 ${code}，${process.env.CODE_TTL_SECONDS || 300} 秒内有效。`,
    });
}

module.exports = {
    isMockMode,
    sendVerifyCode,
};
