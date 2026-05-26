import React from 'react'
import { Zap, Terminal, ChevronRight, Globe } from 'lucide-react'
import { cn } from '../../utils/helpers'
import { LANGUAGE_OPTIONS } from '../../i18n'
import { useI18n } from '../../useI18n'

const SettingRow = ({ title, description, children, icon: Icon }) => (
  <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-6 p-8 bg-white/[0.03] rounded-3xl border border-white/10 hover:border-ps-blue/30 transition-all group">
    <div className="flex items-center space-x-6 min-w-0">
      {Icon && (
        <div className="p-4 bg-white/5 rounded-2xl group-hover:bg-ps-blue/10 transition-colors">
          <Icon className="w-6 h-6 text-zinc-500 group-hover:text-ps-blue transition-colors" />
        </div>
      )}
      <div className="space-y-1 min-w-0">
        <p className="font-bold text-white uppercase text-lg tracking-tight">{title}</p>
        <p className="text-sm text-zinc-500 max-w-md">{description}</p>
      </div>
    </div>
    <div className="shrink-0 md:ml-8">
      {children}
    </div>
  </div>
)

const SettingsView = ({ config, onSaveConfig, setShowLogs }) => {
  const { language, t } = useI18n()
  const autoOpen = config.AUTO_BROWSER_OPEN !== false
  const autoInstall = config.AUTO_INSTALL_APP !== false
  const autoloadDelay = config.AUTOLOAD_DELAY || 5

  return (
    <div className="max-w-5xl space-y-16 pb-20">
      <div className="space-y-4">
        <h2 className="text-4xl font-extrabold text-white tracking-tight">
          {t('settings.title')}
        </h2>
      </div>

      {/* Startup Settings */}
      <section className="space-y-8">
        <h3 className="label-caps !text-ps-blue !opacity-100 flex items-center space-x-4 text-xl tracking-[0.2em]">
          <Zap className="w-6 h-6" />
          <span>{t('settings.startupSection')}</span>
        </h3>

        <div className="space-y-4">
          <SettingRow
            title={t('settings.autoOpen.title')}
            description={t('settings.autoOpen.description')}
          >
            <button
              onClick={() => onSaveConfig({ AUTO_BROWSER_OPEN: !autoOpen })}
              className={cn(
                "w-20 h-10 rounded-full transition-all relative p-1.5",
                autoOpen ? "bg-ps-blue" : "bg-white/10"
              )}
            >
              <div className={cn(
                "w-7 h-7 bg-white rounded-full transition-all",
                autoOpen ? "translate-x-10" : "translate-x-0"
              )} />
            </button>
          </SettingRow>

          <SettingRow
            title={t('settings.autoInstall.title')}
            description={t('settings.autoInstall.description')}
          >
            <button
              onClick={() => onSaveConfig({ AUTO_INSTALL_APP: !autoInstall })}
              className={cn(
                "w-20 h-10 rounded-full transition-all relative p-1.5",
                autoInstall ? "bg-ps-blue" : "bg-white/10"
              )}
            >
              <div className={cn(
                "w-7 h-7 bg-white rounded-full transition-all",
                autoInstall ? "translate-x-10" : "translate-x-0"
              )} />
            </button>
          </SettingRow>

          <SettingRow
            title={t('settings.killDisc.title')}
            description={t('settings.killDisc.description')}
          >
            <button
              onClick={() => onSaveConfig({ KILL_DISC_PLAYER_ON_STARTUP: !config.KILL_DISC_PLAYER_ON_STARTUP })}
              className={cn(
                "w-20 h-10 rounded-full transition-all relative p-1.5",
                config.KILL_DISC_PLAYER_ON_STARTUP !== false ? "bg-ps-blue" : "bg-white/10"
              )}
            >
              <div className={cn(
                "w-7 h-7 bg-white rounded-full transition-all",
                config.KILL_DISC_PLAYER_ON_STARTUP !== false ? "translate-x-10" : "translate-x-0"
              )} />
            </button>
          </SettingRow>

          <SettingRow
            title={t('settings.scanUsb.title')}
            description={t('settings.scanUsb.description')}
          >
            <button
              onClick={() => onSaveConfig({ SCAN_USB_PAYLOADS: !config.SCAN_USB_PAYLOADS })}
              className={cn(
                "w-20 h-10 rounded-full transition-all relative p-1.5",
                config.SCAN_USB_PAYLOADS ? "bg-ps-blue" : "bg-white/10"
              )}
            >
              <div className={cn(
                "w-7 h-7 bg-white rounded-full transition-all",
                config.SCAN_USB_PAYLOADS ? "translate-x-10" : "translate-x-0"
              )} />
            </button>
          </SettingRow>

          <div className="p-8 bg-white/[0.03] rounded-3xl border border-white/10 space-y-8">
            <div className="flex justify-between items-center">
              <div className="space-y-1">
                <p className="font-bold text-white uppercase text-lg tracking-tight">{t('settings.autoloadDelay.title')}</p>
                <p className="text-sm text-zinc-500">{t('settings.autoloadDelay.description')}</p>
              </div>
              <span className="text-ps-blue font-black text-4xl italic tracking-tighter">{autoloadDelay}s</span>
            </div>

            <div className="grid grid-cols-3 gap-4">
              {[3, 5, 10].map(s => (
                <button
                  key={s}
                  onClick={() => onSaveConfig({ AUTOLOAD_DELAY: s })}
                  className={cn(
                    "py-5 rounded-2xl font-black text-xl transition-all border uppercase italic",
                    autoloadDelay === s
                      ? "bg-ps-blue border-ps-blue text-white scale-[1.02]"
                      : "bg-white/5 border-white/10 text-zinc-500 hover:bg-white/10 hover:text-white"
                  )}
                >
                  {s}s
                </button>
              ))}
            </div>
          </div>

          <SettingRow
            title={t('settings.language.title')}
            description={t('settings.language.description')}
            icon={Globe}
          >
            <div className="grid grid-cols-2 gap-2 p-1.5 bg-white/5 rounded-2xl border border-white/10 min-w-64">
              {LANGUAGE_OPTIONS.map(option => (
                <button
                  key={option.value}
                  onClick={() => onSaveConfig({ LANGUAGE: option.value })}
                  className={cn(
                    "px-4 py-3 rounded-xl font-black text-sm transition-all uppercase tracking-tight",
                    language === option.value
                      ? "bg-ps-blue text-white shadow-[0_0_18px_rgba(0,112,209,0.25)]"
                      : "text-zinc-500 hover:text-white hover:bg-white/10"
                  )}
                >
                  {option.label}
                </button>
              ))}
            </div>
          </SettingRow>
        </div>
      </section>

      {/* Diagnostics */}
      <section className="space-y-8">
        <h3 className="label-caps !text-ps-blue !opacity-100 flex items-center space-x-4 text-xl tracking-[0.2em]">
          <Terminal className="w-6 h-6" />
          <span>{t('settings.diagnosticsSection')}</span>
        </h3>

        <button
          onClick={() => setShowLogs(true)}
          className="w-full group flex items-center justify-between p-8 bg-white/[0.03] rounded-3xl border border-white/10 hover:border-ps-blue/50 hover:bg-ps-blue/5 transition-all text-left"
        >
          <div className="flex items-center space-x-6">
            <div className="p-4 bg-white/5 rounded-2xl group-hover:bg-ps-blue/10 transition-colors">
              <Terminal className="w-6 h-6 text-zinc-500 group-hover:text-ps-blue transition-colors" />
            </div>
            <div className="space-y-1">
              <p className="font-bold text-white uppercase text-lg tracking-tight">{t('settings.openLogs.title')}</p>
              <p className="text-sm text-zinc-500 max-w-md">{t('settings.openLogs.description')}</p>
            </div>
          </div>
          <ChevronRight className="w-8 h-8 text-zinc-700 group-hover:text-ps-blue group-hover:translate-x-2 transition-all" />
        </button>
      </section>

    </div>
  )
}

export default SettingsView
