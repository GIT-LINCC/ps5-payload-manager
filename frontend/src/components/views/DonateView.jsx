import React, { useState } from 'react'
import { Check, Copy, Heart, Wallet } from 'lucide-react'
import { QRCodeSVG } from 'qrcode.react'
import { cn, isPS5 } from '../../utils/helpers'
import { useI18n } from '../../useI18n'

const LINCC_WECHAT_PAY_URL = 'weixin://mdwechat/donate/f2f0YfhLFwwZNNIC7hSEL0ybtySAiJpRyqBXv5ObkrZx1iA'
const LINCC_ALIPAY_URL = 'https://qr.alipay.com/fkx15957bug79deqbnpvm5b'
const SOL_ADDRESS = 'HfgbJg5deVp9B1od3brhpQrcyJXA115AxmvAp41idgam'
const ETH_ADDRESS = '0x508fff69243a487ed6e8821697707af00c5069fe'

const paymentMethods = [
  {
    key: 'wechat',
    labelKey: 'donate.wechat',
    value: LINCC_WECHAT_PAY_URL,
    ringClass: 'border-emerald-500/20 shadow-[0_0_45px_rgba(16,185,129,0.18)]',
    titleClass: 'text-emerald-400',
  },
  {
    key: 'alipay',
    labelKey: 'donate.alipay',
    value: LINCC_ALIPAY_URL,
    ringClass: 'border-sky-500/20 shadow-[0_0_45px_rgba(14,165,233,0.18)]',
    titleClass: 'text-sky-400',
  },
]

const cryptoMethods = [
  {
    key: 'sol',
    labelKey: 'donate.sol',
    value: SOL_ADDRESS,
    ringClass: 'border-violet-500/20 shadow-[0_0_45px_rgba(139,92,246,0.16)]',
    titleClass: 'text-violet-300',
  },
  {
    key: 'eth',
    labelKey: 'donate.eth',
    value: ETH_ADDRESS,
    ringClass: 'border-cyan-500/20 shadow-[0_0_45px_rgba(6,182,212,0.16)]',
    titleClass: 'text-cyan-300',
  },
]

const DonateView = () => {
  const { t } = useI18n()
  const [copiedKey, setCopiedKey] = useState('')
  const qrSize = isPS5 ? 180 : 150

  const copyAddress = async (key, value) => {
    try {
      await navigator.clipboard.writeText(value)
      setCopiedKey(key)
      setTimeout(() => setCopiedKey(''), 1500)
    } catch {
      setCopiedKey('')
    }
  }

  return (
    <div className="flex flex-col items-center min-h-[60vh] text-center space-y-8 md:space-y-12 animate-fade-in max-w-6xl mx-auto py-10 md:py-16">
      <div className="p-8 md:p-12 rounded-[2.5rem] md:rounded-[3.5rem] bg-red-600/10 border border-red-600/20 text-red-500 shadow-2xl">
        <Heart className="w-16 h-16 md:w-24 md:h-24 fill-current animate-pulse" />
      </div>

      <div className="space-y-4 md:space-y-6 px-4">
        <h3 className="text-4xl md:text-6xl font-black text-white uppercase italic tracking-tighter">{t('donate.title')} <span className="text-red-500">{t('donate.titleAccent')}</span></h3>
        <p className="text-lg md:text-2xl text-zinc-400 font-medium leading-relaxed italic max-w-3xl mx-auto">
          {t('donate.description')}
        </p>
      </div>

      <section className="w-full space-y-5 md:space-y-6">
        <h4 className="label-caps !text-white !opacity-100 text-lg md:text-xl tracking-[0.2em]">{t('donate.mobilePayments')}</h4>
        <div className={cn("grid gap-5 md:gap-6", isPS5 ? "grid-cols-2" : "grid-cols-1 sm:grid-cols-2")}>
          {paymentMethods.map(method => (
            <div key={method.key} className={cn("glass-card p-6 md:p-8 rounded-ps-3xl border-white/10 bg-white/[0.02]", method.ringClass)}>
              <div className="space-y-5 md:space-y-6">
                <div className="flex items-center justify-center space-x-3">
                  <Wallet className={cn("w-6 h-6", method.titleClass)} />
                  <h5 className={cn("font-black text-2xl md:text-3xl uppercase italic tracking-tight", method.titleClass)}>{t(method.labelKey)}</h5>
                </div>
                <div className="inline-flex bg-white p-4 md:p-5 rounded-[1.75rem]">
                  <QRCodeSVG value={method.value} size={qrSize} level="H" />
                </div>
                <p className="text-xs md:text-sm text-zinc-500 font-bold uppercase tracking-widest">{t('donate.scanToDonate')}</p>
              </div>
            </div>
          ))}
        </div>
      </section>

      <section className="w-full space-y-5 md:space-y-6">
        <h4 className="label-caps !text-white !opacity-100 text-lg md:text-xl tracking-[0.2em]">{t('donate.cryptoPayments')}</h4>
        <div className={cn("grid gap-5 md:gap-6", isPS5 ? "grid-cols-2" : "grid-cols-1 sm:grid-cols-2")}>
          {cryptoMethods.map(method => (
            <div key={method.key} className={cn("glass-card p-6 md:p-8 rounded-ps-3xl border-white/10 bg-white/[0.02]", method.ringClass)}>
              <div className="space-y-5 md:space-y-6">
                <h5 className={cn("font-black text-2xl md:text-3xl uppercase italic tracking-tight", method.titleClass)}>{t(method.labelKey)}</h5>
                <div className="inline-flex bg-white p-4 md:p-5 rounded-[1.75rem]">
                  <QRCodeSVG value={method.value} size={qrSize} level="H" />
                </div>
                <div className="space-y-3">
                  <p className="text-xs text-zinc-500 font-bold uppercase tracking-widest">{t('donate.walletAddress')}</p>
                  <code className="block px-4 py-3 bg-black/30 border border-white/10 rounded-2xl text-zinc-200 text-sm md:text-base break-all font-mono text-left">
                    {method.value}
                  </code>
                  {!isPS5 && (
                    <button
                      onClick={() => copyAddress(method.key, method.value)}
                      className="inline-flex items-center justify-center space-x-2 px-5 py-3 rounded-xl bg-white/5 hover:bg-white/10 border border-white/10 text-white font-bold uppercase text-xs tracking-widest transition-all"
                    >
                      {copiedKey === method.key ? <Check className="w-4 h-4 text-emerald-400" /> : <Copy className="w-4 h-4" />}
                      <span>{copiedKey === method.key ? t('donate.copied') : t('donate.copy')}</span>
                    </button>
                  )}
                </div>
              </div>
            </div>
          ))}
        </div>
      </section>
    </div>
  )
}

export default DonateView
