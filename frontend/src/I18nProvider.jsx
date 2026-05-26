import React, { useMemo } from 'react'
import { createTranslator, normalizeLanguage } from './i18n'
import { I18nContext } from './i18n-context'

export default function I18nProvider({ language, children }) {
  const normalizedLanguage = normalizeLanguage(language)
  const value = useMemo(() => ({
    language: normalizedLanguage,
    t: createTranslator(normalizedLanguage),
  }), [normalizedLanguage])

  return (
    <I18nContext.Provider value={value}>
      {children}
    </I18nContext.Provider>
  )
}
