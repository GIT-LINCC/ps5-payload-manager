import { createContext } from 'react'
import { DEFAULT_LANGUAGE, createTranslator } from './i18n'

export const I18nContext = createContext({
  language: DEFAULT_LANGUAGE,
  t: createTranslator(DEFAULT_LANGUAGE),
})
